# Use a lightweight GCC image
FROM gcc:12-bookworm

# Install Ncurses dev libs (needed so the 'make' command doesn't fail on client targets)
RUN apt-get update && apt-get install -y libncurses5-dev libncursesw5-dev

# Set working directory
WORKDIR /app

# Copy all project files
COPY . .

# Build the project
RUN make clean && make

# Create the data directory structure (important for permissions)
RUN mkdir -p data/backups

# Expose the port defined in protocol.h (8080)
EXPOSE 8080

# Run the server
CMD ["./bin/server"]