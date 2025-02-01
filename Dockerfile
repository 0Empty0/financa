FROM gcc:latest AS builder
WORKDIR /app
COPY . .
RUN gcc -o financa src/main.c  \
    -D_GNU_SOURCE   \
    -Wall -O2

FROM debian:stable-slim
WORKDIR /app
COPY --from=builder /app/financa .
EXPOSE 8080
CMD ["./financa"]
