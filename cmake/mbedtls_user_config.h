/* What the engine changes in Mbed TLS's default configuration (see cmake/Dependencies.cmake). */

/* Requests run on threads of their own, several at once: the state the library keeps for all of
   them (the PSA crypto that TLS 1.3 is built on) is guarded by its own locks. */
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_PTHREAD

/* No self-tests: the engine never runs them, and their counters (of elliptic curve
   multiplications, say) are globals every TLS thread writes with no lock. */
#undef MBEDTLS_SELF_TEST
