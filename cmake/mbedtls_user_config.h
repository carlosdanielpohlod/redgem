/* What the engine changes in Mbed TLS's default configuration (see cmake/Dependencies.cmake). */

/* Requests run on threads of their own, several at once: the state the library keeps for all of
   them (the PSA crypto that TLS 1.3 is built on) is guarded by its own locks. */
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_PTHREAD
