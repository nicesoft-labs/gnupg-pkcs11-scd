/*
 * Copyright (c) 2006-2007 Zeljko Vrba <zvrba@globalnet.hr>
 * Copyright (c) 2006-2017 Alon Bar-Lev <alon.barlev@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     o Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     o Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     o Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#if defined(ENABLE_GNUTLS)
#include <gnutls/x509.h>
#endif
#if defined(ENABLE_OPENSSL)
#include <openssl/x509.h>
#include <openssl/rsa.h>
#endif
#include <ksba.h>
#include "encoding.h"
#include "keyutil.h"

gpg_err_code_t
keyutil_get_cert_mpi (
	unsigned char *der,
	size_t len,
	gcry_mpi_t *p_n_mpi,
	gcry_mpi_t *p_e_mpi
) {
	gpg_err_code_t error = GPG_ERR_GENERAL;
	gcry_mpi_t n_mpi = NULL;
	gcry_mpi_t e_mpi = NULL;
#if defined(ENABLE_GNUTLS)
	gnutls_x509_crt_t cert = NULL;
	gnutls_datum_t datum = {der, len};
	gnutls_datum_t m = {NULL, 0}, e = {NULL, 0};
#elif defined(ENABLE_OPENSSL)
	X509 *x509 = NULL;
	EVP_PKEY *pubkey = NULL;
	RSA *rsa = NULL;
	const BIGNUM *n, *e;
	char *n_hex = NULL, *e_hex = NULL;
#endif

	*p_n_mpi = NULL;
	*p_e_mpi = NULL;

#if defined(ENABLE_GNUTLS)
	if (gnutls_x509_crt_init (&cert) != GNUTLS_E_SUCCESS) {
		cert = NULL;
		error = GPG_ERR_ENOMEM;
		goto cleanup;
	}

	if (gnutls_x509_crt_import (cert, &datum, GNUTLS_X509_FMT_DER) != GNUTLS_E_SUCCESS) {
		error = GPG_ERR_BAD_CERT;
		goto cleanup;
	}

	if (gnutls_x509_crt_get_pk_rsa_raw (cert, &m, &e) != GNUTLS_E_SUCCESS) {
		error = GPG_ERR_WRONG_PUBKEY_ALGO;
		m.data = NULL;
		e.data = NULL;
		goto cleanup;
	}

	if (
		gcry_mpi_scan(&n_mpi, GCRYMPI_FMT_USG, m.data, m.size, NULL) ||
		gcry_mpi_scan(&e_mpi, GCRYMPI_FMT_USG, e.data, e.size, NULL)
	) {
		error = GPG_ERR_BAD_KEY;
		goto cleanup;
	}
#elif defined(ENABLE_OPENSSL)
	if (!d2i_X509 (&x509, (const unsigned char **)&der, len)) {
		error = GPG_ERR_BAD_CERT;
		goto cleanup;
	}

	if ((pubkey = X509_get_pubkey (x509)) == NULL) {
		error = GPG_ERR_WRONG_PUBKEY_ALGO;
		goto cleanup;
	}

	if ((rsa = EVP_PKEY_get1_RSA(pubkey)) == NULL) {
		error = GPG_ERR_WRONG_PUBKEY_ALGO;
		goto cleanup;
	}

	RSA_get0_key(rsa, &n, &e, NULL);

	n_hex = BN_bn2hex (n);
	e_hex = BN_bn2hex (e);

	if(n_hex == NULL || e_hex == NULL) {
		error = GPG_ERR_BAD_KEY;
		goto cleanup;
	}

	if (
		gcry_mpi_scan (&n_mpi, GCRYMPI_FMT_HEX, n_hex, 0, NULL) ||
		gcry_mpi_scan (&e_mpi, GCRYMPI_FMT_HEX, e_hex, 0, NULL)
	) {
		error = GPG_ERR_BAD_KEY;
		goto cleanup;
	}
#else
#error Invalid configuration.
#endif

	*p_n_mpi = n_mpi;
	n_mpi = NULL;
	*p_e_mpi = e_mpi;
	e_mpi = NULL;
	error = GPG_ERR_NO_ERROR;

cleanup:

	if (n_mpi != NULL) {
		gcry_mpi_release (n_mpi);
		n_mpi = NULL;
	}

	if (e_mpi != NULL) {
		gcry_mpi_release (e_mpi);
		e_mpi = NULL;
	}

#if defined(ENABLE_GNUTLS)

	if (m.data != NULL) {
		gnutls_free (m.data);
		m.data = NULL;
	}

	if (e.data != NULL) {
		gnutls_free (e.data);
		e.data = NULL;
	}

	if (cert != NULL) {
		gnutls_x509_crt_deinit (cert);
		cert = NULL;
	}

#elif defined(ENABLE_OPENSSL)

	if (x509 != NULL) {
		X509_free (x509);
		x509 = NULL;
	}

	if (pubkey != NULL) {
		EVP_PKEY_free(pubkey);
		pubkey = NULL;
	}

	if (rsa != NULL) {
		RSA_free(rsa);
		rsa = NULL;
	}

	if (n_hex != NULL) {
		OPENSSL_free (n_hex);
		n_hex = NULL;
	}

	if (e_hex != NULL) {
		OPENSSL_free (e_hex);
		e_hex = NULL;
	}

#else
#error Invalid configuration.
#endif

	return error;
}
/*
   Convert X.509 RSA/ECC/DSA/GOST public key into gcrypt internal sexp form.
   The result is stored in *sexp, which must be freed when not needed
   anymore. *sexp must be NULL on entry, since it is overwritten.
*/
gpg_err_code_t
keyutil_get_cert_sexp (
	unsigned char *der,
	size_t len,
	gcry_sexp_t *p_sexp
) {
	gpg_err_code_t error = GPG_ERR_GENERAL;
        gcry_mpi_t n_mpi = NULL;
        gcry_mpi_t e_mpi = NULL;
        gcry_sexp_t sexp = NULL;
        gpg_error_t err;
        ksba_sexp_t p;
        ksba_cert_t ks_cert;
        size_t n;

        err = ksba_cert_new (&ks_cert);
        if (err) {
                error = GPG_ERR_BAD_KEY;
                goto cleanup;
        }
        err = ksba_cert_init_from_mem (ks_cert, der, len);
        if (err) {
                error = GPG_ERR_BAD_KEY;
                goto cleanup;
        }

        /* Get the public key from the certificate.  */
        p = ksba_cert_get_public_key (ks_cert);
        n = gcry_sexp_canon_len (p, 0, NULL, NULL);
        if (!n) {
                ksba_free (p);
                error = GPG_ERR_BAD_KEY;
                goto cleanup;
        }

        err = gcry_sexp_sscan (p_sexp, NULL, p, n);
        if (err) {
                ksba_free (p);
                error = GPG_ERR_BAD_KEY;
                goto cleanup;
        }
        ksba_free (p);
        error = GPG_ERR_NO_ERROR;
        goto cleanup;
cleanup:

	if (n_mpi != NULL) {
		gcry_mpi_release (n_mpi);
		n_mpi = NULL;
	}

	if (e_mpi != NULL) {
		gcry_mpi_release (e_mpi);
		e_mpi = NULL;
	}

	if (sexp != NULL) {
		gcry_sexp_release (sexp);
		sexp = NULL;
	}

        return error;
}


#if 0
/**
   Calculate certid for the certificate. The certid is stored as hex-encoded,
   null-terminated string into certid which must be at least 41 bytes long.
   This is very primitive ID, just using the SHA1 of the whole certificate DER
   encoding. Currently not used.
*/
void cert_get_hexgrip(unsigned char *der, size_t len, char *certid)
{
	int ret;
	char grip[20];

	SHA1(der, len, grip);
	ret = bin2hex(hexgrip, 41, grip, 20);
	g_assert(ret == 20);
}
#endif

/** Calculate hex-encoded keygrip of public key in sexp. */
char *keyutil_get_cert_hexgrip (gcry_sexp_t sexp)
{
	char *ret = NULL;
	unsigned char grip[20];

	if (gcry_pk_get_keygrip (sexp, grip)) {
		ret = encoding_bin2hex (grip, sizeof (grip));
	}

	return ret;
}
