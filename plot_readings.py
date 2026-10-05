import serial
import sys
import time
import numpy as np
import multiprocessing as mp
import matplotlib.pyplot as plt
import matplotlib.animation as animation

PORT = "COM4"
BAUD = 921600
SAMPLE_RATE_HZ = 1000
SECONDS_TO_SHOW = 10
WINDOW = SAMPLE_RATE_HZ * SECONDS_TO_SHOW  # 10,000 raw samples

DECIMATE = 100
DOWN_WINDOW = WINDOW // DECIMATE  # 100 plotted points per line
MARGIN_FRAC = 0.15
SHRINK_LOOKBACK_BINS = 20

RAW_FIELDS = ["ax", "ay", "az", "temp", "gx", "gy", "gz"]
FILT_FIELDS = ["ax_f", "ay_f", "az_f", "temp_f", "gx_f", "gy_f", "gz_f"]
ALL_FIELDS = RAW_FIELDS + FILT_FIELDS
FIELD_IDX = {name: i + 2 for i, name in enumerate(ALL_FIELDS)}
PLOT_ORDER = ["ax", "ay", "az", "gx", "gy", "gz", "temp"]


def reader_process(port, baud, shared_arrays, write_idx, bin_seq, running):
    try:
        ser = serial.Serial(port, baud, timeout=0)
    except serial.SerialException as e:
        print(f"[reader] Failed to open {port}: {e}")
        running.value = 0
        return

    ser.reset_input_buffer()
    buf = b""
    bin_accum = {name: 0.0 for name in ALL_FIELDS}
    bin_count = 0
    local_write_idx = 0
    local_bin_seq = 0
    last_report = time.time()

    while running.value:
        n_waiting = ser.in_waiting
        if n_waiting:
            buf += ser.read(n_waiting)
            chunks = buf.split(b"\n")
            buf = chunks[-1]

            for raw_bytes in chunks[:-1]:
                try:
                    decoded = raw_bytes.decode("utf-8", errors="ignore").strip()
                except UnicodeDecodeError:
                    continue
                if not decoded:
                    continue
                fields = decoded.split(",")
                if len(fields) <= max(FIELD_IDX.values()):
                    continue
                try:
                    for name, idx in FIELD_IDX.items():
                        bin_accum[name] += float(fields[idx])
                except ValueError:
                    continue

                bin_count += 1
                if bin_count == DECIMATE:
                    for name in ALL_FIELDS:
                        shared_arrays[name][local_write_idx] = bin_accum[name] / DECIMATE
                        bin_accum[name] = 0.0
                    local_write_idx = (local_write_idx + 1) % DOWN_WINDOW
                    bin_count = 0
                    local_bin_seq += 1
                    write_idx.value = local_write_idx
                    bin_seq.value = local_bin_seq
        else:
            time.sleep(0.001)

        now = time.time()
        if now - last_report > 2.0:
            last_report = now

    ser.close()


def get_ordered_down(shared_arrays, name, write_idx_val):
    arr = np.frombuffer(shared_arrays[name].get_obj(), dtype=np.float64)
    return np.concatenate((arr[write_idx_val:], arr[:write_idx_val]))


def main():
    shared_arrays = {name: mp.Array('d', DOWN_WINDOW) for name in ALL_FIELDS}
    write_idx = mp.Value('i', 0)
    bin_seq = mp.Value('i', 0)
    running = mp.Value('i', 1)

    proc = mp.Process(
        target=reader_process,
        args=(PORT, BAUD, shared_arrays, write_idx, bin_seq, running),
        daemon=True,
    )
    proc.start()

    print(f"Reader started. Showing last {SECONDS_TO_SHOW}s. Close plot window to stop.")

    fig, axes = plt.subplots(len(PLOT_ORDER), 1, figsize=(10, 12), sharex=True)
    raw_lines, filt_lines = {}, {}
    x_vals = np.arange(0, WINDOW, DECIMATE)

    for ax, name in zip(axes, PLOT_ORDER):
        raw_line, = ax.plot(x_vals, np.zeros(DOWN_WINDOW), color="black", alpha=0.5, label="raw")
        filt_line, = ax.plot(x_vals, np.zeros(DOWN_WINDOW), color="red", linewidth=1.5, label="filtered")
        raw_lines[name] = raw_line
        filt_lines[name] = filt_line
        ax.set_ylabel(name)
        ax.set_xlim(0, WINDOW)
        ax.legend(loc="upper right", fontsize="small")

    axes[-1].set_xlabel(f"Sample (last {SECONDS_TO_SHOW}s @ {SAMPLE_RATE_HZ}Hz)")

    ylims = {name: None for name in PLOT_ORDER}
    last_rescale_bin = {name: 0 for name in PLOT_ORDER}

    def update(frame):
        try:
            wi = write_idx.value
            current_bin_seq = bin_seq.value
            ordered = {name: get_ordered_down(shared_arrays, name, wi) for name in ALL_FIELDS}

            for name, ax in zip(PLOT_ORDER, axes):
                raw_series = ordered[name]
                filt_series = ordered[name + "_f"]

                raw_lines[name].set_ydata(raw_series)
                filt_lines[name].set_ydata(filt_series)

                # Robust NumPy min/max evaluations
                data_min = float(np.nanmin([np.nanmin(raw_series), np.nanmin(filt_series)]))
                data_max = float(np.nanmax([np.nanmax(raw_series), np.nanmax(filt_series)]))

                current = ylims[name]
                needs_expand = (current is None) or (data_min < current[0]) or (data_max > current[1])

                if needs_expand:
                    span = data_max - data_min
                    margin = span * MARGIN_FRAC if span > 0 else 1.0
                    new_low, new_high = data_min - margin, data_max + margin
                    ax.set_ylim(new_low, new_high)
                    ylims[name] = (new_low, new_high)
                    last_rescale_bin[name] = current_bin_seq
                else:
                    stale_for = current_bin_seq - last_rescale_bin[name]
                    if stale_for >= DOWN_WINDOW:
                        # Rescale back down using full series range to prevent aggressive clipping
                        span = data_max - data_min
                        margin = span * MARGIN_FRAC if span > 0 else 1.0
                        new_low, new_high = data_min - margin, data_max + margin
                        ax.set_ylim(new_low, new_high)
                        ylims[name] = (new_low, new_high)
                        last_rescale_bin[name] = current_bin_seq

        except Exception as err:
            # Prevent frame error from freezing Matplotlib animation loop
            print(f"[gui error]: {err}")

        return list(raw_lines.values()) + list(filt_lines.values())

    ani = animation.FuncAnimation(fig, update, interval=50, blit=False, cache_frame_data=False)

    try:
        plt.tight_layout()
        plt.show()
    finally:
        running.value = 0
        proc.join(timeout=2)
        print("Stopped.")


if __name__ == "__main__":
    main()