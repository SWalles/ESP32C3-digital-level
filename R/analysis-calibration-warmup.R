
# Read data
train <- read.csv("data/imu_log_20260819_164159.csv")


# Checks
all(diff(train$sampleCount) == 1)

# Alpha-Beta Filter
ab_filter <- function(stream, alpha = 0.5, beta = 0.5, delta = rep(1, length(stream)), guess = c(1, 1)) {
  x <- numeric(length(stream) + 1)
  dx <- numeric(length(stream) + 1)
  predx <- numeric(length(stream))
  
  # Initial guess
  x[1] <- guess[1]
  dx[1] <- guess[2]
  delta <- c(1/1e5, delta)
  
  # Loop
  for (n in seq_along(stream)) {
    predx[n] <- x[n] + delta[n] * dx[n]
    dx[n+1] <- dx[n] + beta * (stream[n] - predx[n]) / delta[n]
    x[n+1] <- predx[n] + alpha * (stream[n] - predx[n])
  }
  
  return(list(x = x[-1], dx = dx[-1]))
}

# Temp
plot_sample <- seq(1, nrow(train), by = 1000)
plot(train$temp_raw[plot_sample], type = "l", ylab = "Temp")

temp_filter <- ab_filter(train$temp_raw, 
                         alpha = 0.5, 
                         beta = 0.5, 
                         delta = diff(train$host_time), 
                         guess = c(1, 1))

lines(temp_filter$x[plot_sample], col = 2)

# Optimise alpha / beta
ma_size <- 1001
ma <- as.numeric(filter(temp_filter$x, rep(1/ma_size, ma_size), sides = 2))
lines(ma[plot_sample], col = 3, lwd = 2)

fn <- function(par) {
  alpha = par[1]
  beta = par[2]
  filt <- ab_filter(train$temp_raw, 
                    alpha = alpha, 
                    beta = beta, 
                    delta = diff(train$host_time), 
                    guess = c(1, 1))
  sum((ma - filt$x)^2, na.rm = TRUE)
}

best <- optim(c(0.5, 0.5), fn = fn)

best_temp_filter <- ab_filter(train$temp_raw, 
                              alpha = best$par[1], 
                              beta = best$par[2], 
                              delta = diff(train$host_time), 
                              guess = c(1, 1))

lines(best_temp_filter$x[plot_sample], col = 4, lwd = 2)
