# Read data
train <- read.csv("data/imu_log_20260819_164159.csv")

# Filter
# ---- Alpha-beta filter (integer, shift-based) — mirrors the C++ version ----
# Returns a filtered vector given a vector of raw measurements.
# alpha_shift / beta_shift: alpha = 1/2^alpha_shift, beta = 1/2^beta_shift

alpha_beta_filter <- function(measurements, alpha_shift = 5, beta_shift = 5) {
  n <- length(measurements)
  x_est <- 0L
  v_est <- 0L
  x_rem <- 0L
  v_rem <- 0L
  initialized <- FALSE
  
  out <- integer(n)
  
  for (i in seq_len(n)) {
    m <- as.integer(measurements[i])
    
    if (!initialized) {
      x_est <- m
      v_est <- 0L
      initialized <- TRUE
      out[i] <- x_est
      next
    }
    
    x_predicted <- x_est + v_est          # dt = 1
    residual <- m - x_predicted
    
    x_rem <- x_rem + residual
    x_corr <- bitwShiftR_signed(x_rem, alpha_shift)
    x_rem <- x_rem - bitwShiftL(x_corr, alpha_shift)
    
    v_rem <- v_rem + residual
    v_corr <- bitwShiftR_signed(v_rem, beta_shift)
    v_rem <- v_rem - bitwShiftL(v_corr, beta_shift)
    
    x_est <- x_predicted + x_corr
    v_est <- v_est + v_corr
    
    out[i] <- x_est
  }
  
  out
}

# ---- R's bitwShiftR does NOT sign-extend negative numbers correctly ----
# (it operates on the raw bit pattern the wrong way for negative ints in some
# R builds), so we need a proper arithmetic right shift that floors toward
# -Inf, matching C's behavior on signed integers.
bitwShiftR_signed <- function(x, n) {
  as.integer(floor(x / (2^n)))
}

# =============
# TESTING
# =============

plot_filters <- function(raw, a_shifts, b_shifts) {
  plot_sample <- seq(1, length(raw), length = 1000)
  opar <- par(mfrow = c(length(a_shifts), length(b_shits)), mar = c(0,0,2,0))
  for (i in a_shifts) {
    for (j in b_shits) {
      plot(raw[plot_sample], type = "l", xlab = "", ylab = "", main = paste0("alpha_shift=", i, ", beta_shift=", j))
      lines(alpha_beta_filter(raw, alpha_shift = i, beta_shift = j)[plot_sample], col = 2)
    }
  }
  par(opar)
}

# Temp
a_shifts <- 2:6
b_shits <- 6:10
plot_filters(train$temp_raw, a_shifts, b_shifts)

# Gyro
a_shifts <- 5:8
b_shits <- 8:11
plot_filters(train$gx_raw[10000:nrow(train)], a_shifts, b_shifts) # gx
plot_filters(train$gy_raw[10000:nrow(train)], a_shifts, b_shifts) # gy
plot_filters(train$gz_raw[10000:nrow(train)], a_shifts, b_shifts) # gz

# Accel
a_shifts <- 5:8
b_shits <- 9:12
plot_filters(train$ax_raw[10000:nrow(train)], a_shifts, b_shifts) # ax
plot_filters(train$ay_raw[10000:nrow(train)], a_shifts, b_shifts) # ay
plot_filters(train$az_raw[10000:nrow(train)], a_shifts, b_shifts) # az




