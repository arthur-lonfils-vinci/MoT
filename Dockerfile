# ============================
# Stage 1: Builder
# ============================
FROM gcc:12-bookworm AS builder

# Install dev dependencies needed for compilation
RUN apt-get update && apt-get install -y \
    libncurses5-dev \
    libncursesw5-dev \
    libssl-dev \
		libnotify-dev \
    libsqlite3-dev

WORKDIR /app

# Copy all project files
COPY . .

# Build the project
# We clean first to ensure no artifact mismatch
RUN make clean && make

# ============================
# Stage 2: Runtime
# ============================
# Use a slim image for production (much smaller size)
FROM debian:bookworm-slim

# Install only runtime libraries (no compiler)
RUN apt-get update && apt-get install -y \
    libsqlite3-0 \
    libssl3 \
    openssl \
    ca-certificates \
		libnotify-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled server binary from the builder stage
# (Note: make produces binaries in ./bin/)
COPY --from=builder /app/bin/server ./server

# Copy the entrypoint script
COPY scripts/docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# Create data directory structure
RUN mkdir -p data/backups

# Expose the default port (Documentation only)
EXPOSE 8080

# Set the Entrypoint to our script
ENTRYPOINT ["docker-entrypoint.sh"]

# Default command to run (passed to entrypoint)
CMD ["./server"]