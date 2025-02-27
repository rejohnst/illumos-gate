/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2017 Nexenta Systems, Inc.  All rights reserved.
 * Copyright 2019 Joyent, Inc.
 */

/*
 * AES provider for the Kernel Cryptographic Framework (KCF)
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/spi.h>
#include <sys/sysmacros.h>
#include <sys/strsun.h>
#include <modes/modes.h>
#define	_AES_IMPL
#include <aes/aes_impl.h>

extern struct mod_ops mod_cryptoops;

/*
 * Module linkage information for the kernel.
 */
static struct modlcrypto modlcrypto = {
	&mod_cryptoops,
	"AES Kernel SW Provider"
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlcrypto,
	NULL
};

/*
 * Mechanism info structure passed to KCF during registration.
 */
static crypto_mech_info_t aes_mech_info_tab[] = {
	/* AES_ECB */
	{SUN_CKM_AES_ECB, AES_ECB_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    AES_MIN_KEY_BYTES, AES_MAX_KEY_BYTES, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* AES_CBC */
	{SUN_CKM_AES_CBC, AES_CBC_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    AES_MIN_KEY_BYTES, AES_MAX_KEY_BYTES, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* AES_CMAC */
	{SUN_CKM_AES_CMAC, AES_CMAC_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_MAC | CRYPTO_FG_MAC_ATOMIC |
	    AES_MIN_KEY_BYTES, AES_MAX_KEY_BYTES, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* AES_CTR */
	{SUN_CKM_AES_CTR, AES_CTR_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    AES_MIN_KEY_BYTES, AES_MAX_KEY_BYTES, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* AES_CCM */
	{SUN_CKM_AES_CCM, AES_CCM_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    AES_MIN_KEY_BYTES, AES_MAX_KEY_BYTES, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* AES_GCM */
	{SUN_CKM_AES_GCM, AES_GCM_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    AES_MIN_KEY_BYTES, AES_MAX_KEY_BYTES, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* AES_GMAC */
	{SUN_CKM_AES_GMAC, AES_GMAC_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC |
	    CRYPTO_FG_MAC | CRYPTO_FG_MAC_ATOMIC |
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC,
	    AES_MIN_KEY_BYTES, AES_MAX_KEY_BYTES, CRYPTO_KEYSIZE_UNIT_IN_BYTES}
};

/* operations are in-place if the output buffer is NULL */
#define	AES_ARG_INPLACE(input, output)				\
	if ((output) == NULL)					\
		(output) = (input);

static void aes_provider_status(crypto_provider_handle_t, uint_t *);

static crypto_control_ops_t aes_control_ops = {
	aes_provider_status
};

static int aes_encrypt_init(crypto_ctx_t *, crypto_mechanism_t *,
    crypto_key_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);
static int aes_decrypt_init(crypto_ctx_t *, crypto_mechanism_t *,
    crypto_key_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);
static int aes_common_init(crypto_ctx_t *, crypto_mechanism_t *,
    crypto_key_t *, crypto_spi_ctx_template_t, crypto_req_handle_t, boolean_t);
static int aes_common_init_ctx(aes_ctx_t *, crypto_spi_ctx_template_t *,
    crypto_mechanism_t *, crypto_key_t *, int, boolean_t);
static int aes_encrypt_final(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int aes_decrypt_final(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);

static int aes_encrypt(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int aes_encrypt_update(crypto_ctx_t *, crypto_data_t *,
    crypto_data_t *, crypto_req_handle_t);
static int aes_encrypt_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *,
    crypto_data_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);

static int aes_decrypt(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int aes_decrypt_update(crypto_ctx_t *, crypto_data_t *,
    crypto_data_t *, crypto_req_handle_t);
static int aes_decrypt_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *,
    crypto_data_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);

static crypto_cipher_ops_t aes_cipher_ops = {
	aes_encrypt_init,
	aes_encrypt,
	aes_encrypt_update,
	aes_encrypt_final,
	aes_encrypt_atomic,
	aes_decrypt_init,
	aes_decrypt,
	aes_decrypt_update,
	aes_decrypt_final,
	aes_decrypt_atomic
};

static int aes_mac_init(crypto_ctx_t *, crypto_mechanism_t *,
    crypto_key_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);
static int aes_mac(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int aes_mac_update(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int aes_mac_final(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int aes_mac_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *, crypto_data_t *,
    crypto_spi_ctx_template_t, crypto_req_handle_t);
static int aes_mac_verify_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *, crypto_data_t *,
    crypto_spi_ctx_template_t, crypto_req_handle_t);

static crypto_mac_ops_t aes_mac_ops = {
	aes_mac_init,
	aes_mac,
	aes_mac_update,
	aes_mac_final,
	aes_mac_atomic,
	aes_mac_verify_atomic
};

static int aes_create_ctx_template(crypto_provider_handle_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_spi_ctx_template_t *,
    size_t *, crypto_req_handle_t);
static int aes_free_context(crypto_ctx_t *);

static crypto_ctx_ops_t aes_ctx_ops = {
	aes_create_ctx_template,
	aes_free_context
};

static crypto_ops_t aes_crypto_ops = {
	&aes_control_ops,
	NULL,
	&aes_cipher_ops,
	&aes_mac_ops,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&aes_ctx_ops,
	NULL,
	NULL,
	NULL,
};

static crypto_provider_info_t aes_prov_info = {
	CRYPTO_SPI_VERSION_4,
	"AES Software Provider",
	CRYPTO_SW_PROVIDER,
	{&modlinkage},
	NULL,
	&aes_crypto_ops,
	sizeof (aes_mech_info_tab)/sizeof (crypto_mech_info_t),
	aes_mech_info_tab
};

static crypto_kcf_provider_handle_t aes_prov_handle = 0;
static crypto_data_t null_crypto_data = { CRYPTO_DATA_RAW };

int
_init(void)
{
	int ret;

	if ((ret = mod_install(&modlinkage)) != 0)
		return (ret);

	/* Register with KCF.  If the registration fails, remove the module. */
	if (crypto_register_provider(&aes_prov_info, &aes_prov_handle)) {
		(void) mod_remove(&modlinkage);
		return (EACCES);
	}

	return (0);
}

int
_fini(void)
{
	/* Unregister from KCF if module is registered */
	if (aes_prov_handle != 0) {
		if (crypto_unregister_provider(aes_prov_handle))
			return (EBUSY);

		aes_prov_handle = 0;
	}

	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


static int
aes_check_mech_param(crypto_mechanism_t *mechanism, aes_ctx_t **ctx, int kmflag)
{
	void *p = NULL;
	boolean_t param_required = B_TRUE;
	size_t param_len;
	void *(*alloc_fun)(int);
	int rv = CRYPTO_SUCCESS;

	switch (mechanism->cm_type) {
	case AES_ECB_MECH_INFO_TYPE:
		param_required = B_FALSE;
		alloc_fun = ecb_alloc_ctx;
		break;
	case AES_CBC_MECH_INFO_TYPE:
		param_len = AES_BLOCK_LEN;
		alloc_fun = cbc_alloc_ctx;
		break;
	case AES_CMAC_MECH_INFO_TYPE:
		param_required = B_FALSE;
		alloc_fun = cmac_alloc_ctx;
		break;
	case AES_CTR_MECH_INFO_TYPE:
		param_len = sizeof (CK_AES_CTR_PARAMS);
		alloc_fun = ctr_alloc_ctx;
		break;
	case AES_CCM_MECH_INFO_TYPE:
		param_len = sizeof (CK_AES_CCM_PARAMS);
		alloc_fun = ccm_alloc_ctx;
		break;
	case AES_GCM_MECH_INFO_TYPE:
		param_len = sizeof (CK_AES_GCM_PARAMS);
		alloc_fun = gcm_alloc_ctx;
		break;
	case AES_GMAC_MECH_INFO_TYPE:
		param_len = sizeof (CK_AES_GMAC_PARAMS);
		alloc_fun = gmac_alloc_ctx;
		break;
	default:
		rv = CRYPTO_MECHANISM_INVALID;
		return (rv);
	}
	if (param_required && mechanism->cm_param != NULL &&
	    mechanism->cm_param_len != param_len) {
		rv = CRYPTO_MECHANISM_PARAM_INVALID;
	}
	if (ctx != NULL) {
		p = (alloc_fun)(kmflag);
		*ctx = p;
	}
	return (rv);
}

/*
 * Initialize key schedules for AES
 */
static int
init_keysched(crypto_key_t *key, void *newbie)
{
	/*
	 * Only keys by value are supported by this module.
	 */
	switch (key->ck_format) {
	case CRYPTO_KEY_RAW:
		if (key->ck_length < AES_MINBITS ||
		    key->ck_length > AES_MAXBITS) {
			return (CRYPTO_KEY_SIZE_RANGE);
		}

		/* key length must be either 128, 192, or 256 */
		if ((key->ck_length & 63) != 0)
			return (CRYPTO_KEY_SIZE_RANGE);
		break;
	default:
		return (CRYPTO_KEY_TYPE_INCONSISTENT);
	}

	aes_init_keysched(key->ck_data, key->ck_length, newbie);
	return (CRYPTO_SUCCESS);
}

/*
 * KCF software provider control entry points.
 */
/* ARGSUSED */
static void
aes_provider_status(crypto_provider_handle_t provider, uint_t *status)
{
	*status = CRYPTO_PROVIDER_READY;
}

static int
aes_encrypt_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t template,
    crypto_req_handle_t req)
{
	return (aes_common_init(ctx, mechanism, key, template, req, B_TRUE));
}

static int
aes_decrypt_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t template,
    crypto_req_handle_t req)
{
	return (aes_common_init(ctx, mechanism, key, template, req, B_FALSE));
}



/*
 * KCF software provider encrypt entry points.
 */
static int
aes_common_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t template,
    crypto_req_handle_t req, boolean_t is_encrypt_init)
{
	aes_ctx_t *aes_ctx;
	int rv;
	int kmflag;

	/*
	 * Only keys by value are supported by this module.
	 */
	if (key->ck_format != CRYPTO_KEY_RAW) {
		return (CRYPTO_KEY_TYPE_INCONSISTENT);
	}

	kmflag = crypto_kmflag(req);
	if ((rv = aes_check_mech_param(mechanism, &aes_ctx, kmflag))
	    != CRYPTO_SUCCESS)
		return (rv);

	rv = aes_common_init_ctx(aes_ctx, template, mechanism, key, kmflag,
	    is_encrypt_init);
	if (rv != CRYPTO_SUCCESS) {
		crypto_free_mode_ctx(aes_ctx);
		return (rv);
	}

	ctx->cc_provider_private = aes_ctx;

	return (CRYPTO_SUCCESS);
}

static int
aes_encrypt(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_data_t *ciphertext, crypto_req_handle_t req)
{
	int ret = CRYPTO_FAILED;

	aes_ctx_t *aes_ctx;
	size_t saved_length, saved_offset, length_needed;

	ASSERT(ctx->cc_provider_private != NULL);
	aes_ctx = ctx->cc_provider_private;

	/*
	 * For block ciphers, plaintext must be a multiple of AES block size.
	 * This test is only valid for ciphers whose blocksize is a power of 2.
	 */
	if (((aes_ctx->ac_flags & (CMAC_MODE|CTR_MODE|CCM_MODE|
	    GCM_MODE|GMAC_MODE)) == 0) &&
	    (plaintext->cd_length & (AES_BLOCK_LEN - 1)) != 0)
		return (CRYPTO_DATA_LEN_RANGE);

	AES_ARG_INPLACE(plaintext, ciphertext);

	/*
	 * We need to just return the length needed to store the output.
	 * We should not destroy the context for the following case.
	 */
	switch (aes_ctx->ac_flags & (CMAC_MODE|CCM_MODE|GCM_MODE|GMAC_MODE)) {
	case CCM_MODE:
		length_needed = plaintext->cd_length + aes_ctx->ac_mac_len;
		break;
	case GCM_MODE:
		length_needed = plaintext->cd_length + aes_ctx->ac_tag_len;
		break;
	case CMAC_MODE:
		length_needed = AES_BLOCK_LEN;
		break;
	case GMAC_MODE:
		if (plaintext->cd_length != 0)
			return (CRYPTO_ARGUMENTS_BAD);

		length_needed = aes_ctx->ac_tag_len;
		break;
	default:
		length_needed = plaintext->cd_length;
	}

	if (ciphertext->cd_length < length_needed) {
		ciphertext->cd_length = length_needed;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	saved_length = ciphertext->cd_length;
	saved_offset = ciphertext->cd_offset;

	/*
	 * Do an update on the specified input data.
	 */
	ret = aes_encrypt_update(ctx, plaintext, ciphertext, req);
	if (ret != CRYPTO_SUCCESS) {
		return (ret);
	}

	/*
	 * For CCM mode, aes_ccm_encrypt_final() will take care of any
	 * left-over unprocessed data, and compute the MAC
	 */
	if (aes_ctx->ac_flags & CCM_MODE) {
		/*
		 * ccm_encrypt_final() will compute the MAC and append
		 * it to existing ciphertext. So, need to adjust the left over
		 * length value accordingly
		 */

		/* order of following 2 lines MUST not be reversed */
		ciphertext->cd_offset = ciphertext->cd_length;
		ciphertext->cd_length = saved_length - ciphertext->cd_length;
		ret = ccm_encrypt_final((ccm_ctx_t *)aes_ctx, ciphertext,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
		if (ret != CRYPTO_SUCCESS) {
			return (ret);
		}

		if (plaintext != ciphertext) {
			ciphertext->cd_length =
			    ciphertext->cd_offset - saved_offset;
		}
		ciphertext->cd_offset = saved_offset;
	} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
		/*
		 * gcm_encrypt_final() will compute the MAC and append
		 * it to existing ciphertext. So, need to adjust the left over
		 * length value accordingly
		 */

		/* order of following 2 lines MUST not be reversed */
		ciphertext->cd_offset = ciphertext->cd_length;
		ciphertext->cd_length = saved_length - ciphertext->cd_length;
		ret = gcm_encrypt_final((gcm_ctx_t *)aes_ctx, ciphertext,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
		if (ret != CRYPTO_SUCCESS) {
			return (ret);
		}

		if (plaintext != ciphertext) {
			ciphertext->cd_length =
			    ciphertext->cd_offset - saved_offset;
		}
		ciphertext->cd_offset = saved_offset;
	} else if (aes_ctx->ac_flags & CMAC_MODE) {
		/* cmac_update doesn't store data */
		ciphertext->cd_length = saved_length;
		ret = cmac_mode_final((cbc_ctx_t *)aes_ctx, ciphertext,
		    aes_encrypt_block, aes_xor_block);
		aes_ctx->ac_remainder_len = 0;
	}

	ASSERT(aes_ctx->ac_remainder_len == 0);
	(void) aes_free_context(ctx);

	return (ret);
}


static int
aes_decrypt(crypto_ctx_t *ctx, crypto_data_t *ciphertext,
    crypto_data_t *plaintext, crypto_req_handle_t req)
{
	int ret = CRYPTO_FAILED;

	aes_ctx_t *aes_ctx;
	off_t saved_offset;
	size_t saved_length, length_needed;

	ASSERT(ctx->cc_provider_private != NULL);
	aes_ctx = ctx->cc_provider_private;

	/*
	 * For block ciphers, plaintext must be a multiple of AES block size.
	 * This test is only valid for ciphers whose blocksize is a power of 2.
	 */
	if (((aes_ctx->ac_flags & (CTR_MODE|CCM_MODE|GCM_MODE|GMAC_MODE))
	    == 0) && (ciphertext->cd_length & (AES_BLOCK_LEN - 1)) != 0) {
		return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);
	}

	AES_ARG_INPLACE(ciphertext, plaintext);

	/*
	 * Return length needed to store the output.
	 * Do not destroy context when plaintext buffer is too small.
	 *
	 * CCM:  plaintext is MAC len smaller than cipher text
	 * GCM:  plaintext is TAG len smaller than cipher text
	 * GMAC: plaintext length must be zero
	 */
	switch (aes_ctx->ac_flags & (CCM_MODE|GCM_MODE|GMAC_MODE)) {
	case CCM_MODE:
		length_needed = aes_ctx->ac_processed_data_len;
		break;
	case GCM_MODE:
		length_needed = ciphertext->cd_length - aes_ctx->ac_tag_len;
		break;
	case GMAC_MODE:
		if (plaintext->cd_length != 0)
			return (CRYPTO_ARGUMENTS_BAD);

		length_needed = 0;
		break;
	default:
		length_needed = ciphertext->cd_length;
	}

	if (plaintext->cd_length < length_needed) {
		plaintext->cd_length = length_needed;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	saved_offset = plaintext->cd_offset;
	saved_length = plaintext->cd_length;

	/*
	 * Do an update on the specified input data.
	 */
	ret = aes_decrypt_update(ctx, ciphertext, plaintext, req);
	if (ret != CRYPTO_SUCCESS) {
		goto cleanup;
	}

	if (aes_ctx->ac_flags & CCM_MODE) {
		ASSERT(aes_ctx->ac_processed_data_len == aes_ctx->ac_data_len);
		ASSERT(aes_ctx->ac_processed_mac_len == aes_ctx->ac_mac_len);

		/* order of following 2 lines MUST not be reversed */
		plaintext->cd_offset = plaintext->cd_length;
		plaintext->cd_length = saved_length - plaintext->cd_length;

		ret = ccm_decrypt_final((ccm_ctx_t *)aes_ctx, plaintext,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
		if (ret == CRYPTO_SUCCESS) {
			if (plaintext != ciphertext) {
				plaintext->cd_length =
				    plaintext->cd_offset - saved_offset;
			}
		} else {
			plaintext->cd_length = saved_length;
		}

		plaintext->cd_offset = saved_offset;
	} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
		/* order of following 2 lines MUST not be reversed */
		plaintext->cd_offset = plaintext->cd_length;
		plaintext->cd_length = saved_length - plaintext->cd_length;

		ret = gcm_decrypt_final((gcm_ctx_t *)aes_ctx, plaintext,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
		if (ret == CRYPTO_SUCCESS) {
			if (plaintext != ciphertext) {
				plaintext->cd_length =
				    plaintext->cd_offset - saved_offset;
			}
		} else {
			plaintext->cd_length = saved_length;
		}

		plaintext->cd_offset = saved_offset;
	}

	ASSERT(aes_ctx->ac_remainder_len == 0);

cleanup:
	(void) aes_free_context(ctx);

	return (ret);
}


/* ARGSUSED */
static int
aes_encrypt_update(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_data_t *ciphertext, crypto_req_handle_t req)
{
	off_t saved_offset;
	size_t saved_length, out_len;
	int ret = CRYPTO_SUCCESS;
	aes_ctx_t *aes_ctx;

	ASSERT(ctx->cc_provider_private != NULL);
	aes_ctx = ctx->cc_provider_private;

	AES_ARG_INPLACE(plaintext, ciphertext);

	/*
	 * CTR mode does not accumulate plaintext across xx_update() calls --
	 * it always outputs the same number of bytes as the input (so
	 * ac_remainder_len is always 0).  Other modes _do_ accumulate
	 * plaintext, and output only full blocks. For non-CTR modes, adjust
	 * the output size to reflect this.
	 */
	out_len = plaintext->cd_length + aes_ctx->ac_remainder_len;
	if ((aes_ctx->ac_flags & CTR_MODE) == 0)
		out_len &= ~(AES_BLOCK_LEN - 1);

	/*
	 * return length needed to store the output.
	 * CMAC stores its output in a local buffer until *_final.
	 */
	if ((aes_ctx->ac_flags & CMAC_MODE) == 0 &&
	    ciphertext->cd_length < out_len) {
		ciphertext->cd_length = out_len;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	saved_offset = ciphertext->cd_offset;
	saved_length = ciphertext->cd_length;

	/*
	 * Do the AES update on the specified input data.
	 */
	switch (plaintext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = crypto_update_iov(ctx->cc_provider_private,
		    plaintext, ciphertext, aes_encrypt_contiguous_blocks,
		    aes_copy_block64);
		break;
	case CRYPTO_DATA_UIO:
		ret = crypto_update_uio(ctx->cc_provider_private,
		    plaintext, ciphertext, aes_encrypt_contiguous_blocks,
		    aes_copy_block64);
		break;
	case CRYPTO_DATA_MBLK:
		ret = crypto_update_mp(ctx->cc_provider_private,
		    plaintext, ciphertext, aes_encrypt_contiguous_blocks,
		    aes_copy_block64);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (ret == CRYPTO_SUCCESS) {
		if (plaintext != ciphertext) {
			ciphertext->cd_length =
			    ciphertext->cd_offset - saved_offset;
		}
	} else {
		ciphertext->cd_length = saved_length;
	}
	ciphertext->cd_offset = saved_offset;

	return (ret);
}


static int
aes_decrypt_update(crypto_ctx_t *ctx, crypto_data_t *ciphertext,
    crypto_data_t *plaintext, crypto_req_handle_t req)
{
	off_t saved_offset;
	size_t saved_length, out_len;
	int ret = CRYPTO_SUCCESS;
	aes_ctx_t *aes_ctx;

	ASSERT(ctx->cc_provider_private != NULL);
	aes_ctx = ctx->cc_provider_private;

	AES_ARG_INPLACE(ciphertext, plaintext);

	/*
	 * Adjust the number of bytes that will hold the plaintext (out_len).
	 * CCM, GCM, and GMAC mechanisms never return plaintext for update
	 * operations, so we set out_len to 0 for those.
	 *
	 * CTR mode does not accumulate any ciphertext across xx_decrypt
	 * calls, and always outputs as many bytes of plaintext as
	 * ciphertext.
	 *
	 * The remaining mechanisms output full blocks of plaintext, so
	 * we round out_len down to the closest multiple of AES_BLOCK_LEN.
	 */
	out_len = aes_ctx->ac_remainder_len + ciphertext->cd_length;
	if ((aes_ctx->ac_flags & (CCM_MODE|GCM_MODE|GMAC_MODE)) != 0) {
		out_len = 0;
	} else if ((aes_ctx->ac_flags & CTR_MODE) == 0) {
		out_len &= ~(AES_BLOCK_LEN - 1);
	}

	/* return length needed to store the output */
	if (plaintext->cd_length < out_len) {
		plaintext->cd_length = out_len;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	saved_offset = plaintext->cd_offset;
	saved_length = plaintext->cd_length;

	if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE))
		gcm_set_kmflag((gcm_ctx_t *)aes_ctx, crypto_kmflag(req));

	/*
	 * Do the AES update on the specified input data.
	 */
	switch (ciphertext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = crypto_update_iov(ctx->cc_provider_private,
		    ciphertext, plaintext, aes_decrypt_contiguous_blocks,
		    aes_copy_block64);
		break;
	case CRYPTO_DATA_UIO:
		ret = crypto_update_uio(ctx->cc_provider_private,
		    ciphertext, plaintext, aes_decrypt_contiguous_blocks,
		    aes_copy_block64);
		break;
	case CRYPTO_DATA_MBLK:
		ret = crypto_update_mp(ctx->cc_provider_private,
		    ciphertext, plaintext, aes_decrypt_contiguous_blocks,
		    aes_copy_block64);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (ret == CRYPTO_SUCCESS) {
		if (ciphertext != plaintext)
			plaintext->cd_length =
			    plaintext->cd_offset - saved_offset;
	} else {
		plaintext->cd_length = saved_length;
	}
	plaintext->cd_offset = saved_offset;


	return (ret);
}

/* ARGSUSED */
static int
aes_encrypt_final(crypto_ctx_t *ctx, crypto_data_t *data,
    crypto_req_handle_t req)
{
	aes_ctx_t *aes_ctx;
	int ret;

	ASSERT(ctx->cc_provider_private != NULL);
	aes_ctx = ctx->cc_provider_private;

	if (data->cd_format != CRYPTO_DATA_RAW &&
	    data->cd_format != CRYPTO_DATA_UIO &&
	    data->cd_format != CRYPTO_DATA_MBLK) {
		return (CRYPTO_ARGUMENTS_BAD);
	}

	if (aes_ctx->ac_flags & CCM_MODE) {
		ret = ccm_encrypt_final((ccm_ctx_t *)aes_ctx, data,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
		if (ret != CRYPTO_SUCCESS) {
			return (ret);
		}
	} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
		size_t saved_offset = data->cd_offset;

		ret = gcm_encrypt_final((gcm_ctx_t *)aes_ctx, data,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
		if (ret != CRYPTO_SUCCESS) {
			return (ret);
		}
		data->cd_length = data->cd_offset - saved_offset;
		data->cd_offset = saved_offset;
	} else if (aes_ctx->ac_flags & CMAC_MODE) {
		ret = cmac_mode_final((cbc_ctx_t *)aes_ctx, data,
		    aes_encrypt_block, aes_xor_block);
		if (ret != CRYPTO_SUCCESS)
			return (ret);
		data->cd_length = AES_BLOCK_LEN;
	} else if ((aes_ctx->ac_flags & CTR_MODE) == 0) {
		/*
		 * There must be no unprocessed plaintext.
		 * This happens if the length of the last data is
		 * not a multiple of the AES block length.
		 */
		if (aes_ctx->ac_remainder_len > 0) {
			return (CRYPTO_DATA_LEN_RANGE);
		}
		data->cd_length = 0;
	}

	(void) aes_free_context(ctx);

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
aes_decrypt_final(crypto_ctx_t *ctx, crypto_data_t *data,
    crypto_req_handle_t req)
{
	aes_ctx_t *aes_ctx;
	int ret;
	off_t saved_offset;
	size_t saved_length;

	ASSERT(ctx->cc_provider_private != NULL);
	aes_ctx = ctx->cc_provider_private;

	if (data->cd_format != CRYPTO_DATA_RAW &&
	    data->cd_format != CRYPTO_DATA_UIO &&
	    data->cd_format != CRYPTO_DATA_MBLK) {
		return (CRYPTO_ARGUMENTS_BAD);
	}

	/*
	 * There must be no unprocessed ciphertext.
	 * This happens if the length of the last ciphertext is
	 * not a multiple of the AES block length.
	 *
	 * For CTR mode, ac_remainder_len is always zero (we never
	 * accumulate ciphertext across update calls with CTR mode).
	 */
	if (aes_ctx->ac_remainder_len > 0 &&
	    (aes_ctx->ac_flags & CTR_MODE) == 0) {
		return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);
	}

	if (aes_ctx->ac_flags & CCM_MODE) {
		/*
		 * This is where all the plaintext is returned, make sure
		 * the plaintext buffer is big enough
		 */
		size_t pt_len = aes_ctx->ac_data_len;
		if (data->cd_length < pt_len) {
			data->cd_length = pt_len;
			return (CRYPTO_BUFFER_TOO_SMALL);
		}

		ASSERT(aes_ctx->ac_processed_data_len == pt_len);
		ASSERT(aes_ctx->ac_processed_mac_len == aes_ctx->ac_mac_len);
		saved_offset = data->cd_offset;
		saved_length = data->cd_length;
		ret = ccm_decrypt_final((ccm_ctx_t *)aes_ctx, data,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
		if (ret == CRYPTO_SUCCESS) {
			data->cd_length = data->cd_offset - saved_offset;
		} else {
			data->cd_length = saved_length;
		}

		data->cd_offset = saved_offset;
		if (ret != CRYPTO_SUCCESS) {
			return (ret);
		}
	} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
		/*
		 * This is where all the plaintext is returned, make sure
		 * the plaintext buffer is big enough
		 */
		gcm_ctx_t *ctx = (gcm_ctx_t *)aes_ctx;
		size_t pt_len = ctx->gcm_processed_data_len - ctx->gcm_tag_len;

		if (data->cd_length < pt_len) {
			data->cd_length = pt_len;
			return (CRYPTO_BUFFER_TOO_SMALL);
		}

		saved_offset = data->cd_offset;
		saved_length = data->cd_length;
		ret = gcm_decrypt_final((gcm_ctx_t *)aes_ctx, data,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
		if (ret == CRYPTO_SUCCESS) {
			data->cd_length = data->cd_offset - saved_offset;
		} else {
			data->cd_length = saved_length;
		}

		data->cd_offset = saved_offset;
		if (ret != CRYPTO_SUCCESS) {
			return (ret);
		}
	}


	if ((aes_ctx->ac_flags & (CTR_MODE|CCM_MODE|GCM_MODE|GMAC_MODE)) == 0) {
		data->cd_length = 0;
	}

	(void) aes_free_context(ctx);

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
aes_encrypt_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *plaintext, crypto_data_t *ciphertext,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	aes_ctx_t aes_ctx;	/* on the stack */
	off_t saved_offset;
	size_t saved_length;
	size_t length_needed;
	int ret;

	AES_ARG_INPLACE(plaintext, ciphertext);

	/*
	 * CTR, CCM, CMAC, GCM, and GMAC modes do not require that plaintext
	 * be a multiple of AES block size.
	 */
	switch (mechanism->cm_type) {
	case AES_CTR_MECH_INFO_TYPE:
	case AES_CCM_MECH_INFO_TYPE:
	case AES_GCM_MECH_INFO_TYPE:
	case AES_GMAC_MECH_INFO_TYPE:
	case AES_CMAC_MECH_INFO_TYPE:
		break;
	default:
		if ((plaintext->cd_length & (AES_BLOCK_LEN - 1)) != 0)
			return (CRYPTO_DATA_LEN_RANGE);
	}

	if ((ret = aes_check_mech_param(mechanism, NULL, 0)) != CRYPTO_SUCCESS)
		return (ret);

	bzero(&aes_ctx, sizeof (aes_ctx_t));

	ret = aes_common_init_ctx(&aes_ctx, template, mechanism, key,
	    crypto_kmflag(req), B_TRUE);
	if (ret != CRYPTO_SUCCESS)
		return (ret);

	switch (mechanism->cm_type) {
	case AES_CCM_MECH_INFO_TYPE:
		length_needed = plaintext->cd_length + aes_ctx.ac_mac_len;
		break;
	case AES_GMAC_MECH_INFO_TYPE:
		if (plaintext->cd_length != 0)
			return (CRYPTO_ARGUMENTS_BAD);
		/* FALLTHRU */
	case AES_GCM_MECH_INFO_TYPE:
		length_needed = plaintext->cd_length + aes_ctx.ac_tag_len;
		break;
	case AES_CMAC_MECH_INFO_TYPE:
		length_needed = AES_BLOCK_LEN;
		break;
	default:
		length_needed = plaintext->cd_length;
	}

	/* return size of buffer needed to store output */
	if (ciphertext->cd_length < length_needed) {
		ciphertext->cd_length = length_needed;
		ret = CRYPTO_BUFFER_TOO_SMALL;
		goto out;
	}

	saved_offset = ciphertext->cd_offset;
	saved_length = ciphertext->cd_length;

	/*
	 * Do an update on the specified input data.
	 */
	switch (plaintext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = crypto_update_iov(&aes_ctx, plaintext, ciphertext,
		    aes_encrypt_contiguous_blocks, aes_copy_block64);
		break;
	case CRYPTO_DATA_UIO:
		ret = crypto_update_uio(&aes_ctx, plaintext, ciphertext,
		    aes_encrypt_contiguous_blocks, aes_copy_block64);
		break;
	case CRYPTO_DATA_MBLK:
		ret = crypto_update_mp(&aes_ctx, plaintext, ciphertext,
		    aes_encrypt_contiguous_blocks, aes_copy_block64);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (ret == CRYPTO_SUCCESS) {
		switch (mechanism->cm_type) {
		case AES_CCM_MECH_INFO_TYPE:
			ret = ccm_encrypt_final((ccm_ctx_t *)&aes_ctx,
			    ciphertext, AES_BLOCK_LEN, aes_encrypt_block,
			    aes_xor_block);
			if (ret != CRYPTO_SUCCESS)
				goto out;
			ASSERT3U(aes_ctx.ac_remainder_len, ==, 0);
			break;
		case AES_GCM_MECH_INFO_TYPE:
		case AES_GMAC_MECH_INFO_TYPE:
			ret = gcm_encrypt_final((gcm_ctx_t *)&aes_ctx,
			    ciphertext, AES_BLOCK_LEN, aes_encrypt_block,
			    aes_copy_block, aes_xor_block);
			if (ret != CRYPTO_SUCCESS)
				goto out;
			ASSERT3U(aes_ctx.ac_remainder_len, ==, 0);
			break;
		case AES_CTR_MECH_INFO_TYPE:
			/*
			 * Note that this use of the ASSERT3U has a slightly
			 * different meaning than the other uses in the
			 * switch statement. The other uses are to ensure
			 * no unprocessed plaintext remains after encryption
			 * (and that the input plaintext was an exact multiple
			 * of AES_BLOCK_LEN).
			 *
			 * For CTR mode, it is ensuring that no input
			 * plaintext was ever segmented and buffered during
			 * processing (since it's a stream cipher).
			 */
			ASSERT3U(aes_ctx.ac_remainder_len, ==, 0);
			break;
		case AES_CMAC_MECH_INFO_TYPE:
			ret = cmac_mode_final((cbc_ctx_t *)&aes_ctx,
			    ciphertext, aes_encrypt_block,
			    aes_xor_block);
			if (ret != CRYPTO_SUCCESS)
				goto out;
			break;
		default:
			ASSERT3U(aes_ctx.ac_remainder_len, ==, 0);
			break;
		}

		if (plaintext != ciphertext) {
			ciphertext->cd_length =
			    ciphertext->cd_offset - saved_offset;
		}
	} else {
		ciphertext->cd_length = saved_length;
	}
	ciphertext->cd_offset = saved_offset;

out:
	if (aes_ctx.ac_flags & PROVIDER_OWNS_KEY_SCHEDULE) {
		bzero(aes_ctx.ac_keysched, aes_ctx.ac_keysched_len);
		kmem_free(aes_ctx.ac_keysched, aes_ctx.ac_keysched_len);
	}

	return (ret);
}

/* ARGSUSED */
static int
aes_decrypt_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *ciphertext, crypto_data_t *plaintext,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	aes_ctx_t aes_ctx;	/* on the stack */
	off_t saved_offset;
	size_t saved_length;
	size_t length_needed;
	int ret;

	AES_ARG_INPLACE(ciphertext, plaintext);

	/*
	 * CCM, GCM, CTR, and GMAC modes do not require that ciphertext
	 * be a multiple of AES block size.
	 */
	switch (mechanism->cm_type) {
	case AES_CTR_MECH_INFO_TYPE:
	case AES_CCM_MECH_INFO_TYPE:
	case AES_GCM_MECH_INFO_TYPE:
	case AES_GMAC_MECH_INFO_TYPE:
		break;
	default:
		if ((ciphertext->cd_length & (AES_BLOCK_LEN - 1)) != 0)
			return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);
	}

	if ((ret = aes_check_mech_param(mechanism, NULL, 0)) != CRYPTO_SUCCESS)
		return (ret);

	bzero(&aes_ctx, sizeof (aes_ctx_t));

	ret = aes_common_init_ctx(&aes_ctx, template, mechanism, key,
	    crypto_kmflag(req), B_FALSE);
	if (ret != CRYPTO_SUCCESS)
		return (ret);

	switch (mechanism->cm_type) {
	case AES_CCM_MECH_INFO_TYPE:
		length_needed = aes_ctx.ac_data_len;
		break;
	case AES_GCM_MECH_INFO_TYPE:
		length_needed = ciphertext->cd_length - aes_ctx.ac_tag_len;
		break;
	case AES_GMAC_MECH_INFO_TYPE:
		if (plaintext->cd_length != 0)
			return (CRYPTO_ARGUMENTS_BAD);
		length_needed = 0;
		break;
	default:
		length_needed = ciphertext->cd_length;
	}

	/* return size of buffer needed to store output */
	if (plaintext->cd_length < length_needed) {
		plaintext->cd_length = length_needed;
		ret = CRYPTO_BUFFER_TOO_SMALL;
		goto out;
	}

	saved_offset = plaintext->cd_offset;
	saved_length = plaintext->cd_length;

	if (mechanism->cm_type == AES_GCM_MECH_INFO_TYPE ||
	    mechanism->cm_type == AES_GMAC_MECH_INFO_TYPE)
		gcm_set_kmflag((gcm_ctx_t *)&aes_ctx, crypto_kmflag(req));

	/*
	 * Do an update on the specified input data.
	 */
	switch (ciphertext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = crypto_update_iov(&aes_ctx, ciphertext, plaintext,
		    aes_decrypt_contiguous_blocks, aes_copy_block64);
		break;
	case CRYPTO_DATA_UIO:
		ret = crypto_update_uio(&aes_ctx, ciphertext, plaintext,
		    aes_decrypt_contiguous_blocks, aes_copy_block64);
		break;
	case CRYPTO_DATA_MBLK:
		ret = crypto_update_mp(&aes_ctx, ciphertext, plaintext,
		    aes_decrypt_contiguous_blocks, aes_copy_block64);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (ret == CRYPTO_SUCCESS) {
		switch (mechanism->cm_type) {
		case AES_CCM_MECH_INFO_TYPE:
			ASSERT(aes_ctx.ac_processed_data_len
			    == aes_ctx.ac_data_len);
			ASSERT(aes_ctx.ac_processed_mac_len
			    == aes_ctx.ac_mac_len);
			ret = ccm_decrypt_final((ccm_ctx_t *)&aes_ctx,
			    plaintext, AES_BLOCK_LEN, aes_encrypt_block,
			    aes_copy_block, aes_xor_block);
			ASSERT3U(aes_ctx.ac_remainder_len, ==, 0);
			if ((ret == CRYPTO_SUCCESS) &&
			    (ciphertext != plaintext)) {
				plaintext->cd_length =
				    plaintext->cd_offset - saved_offset;
			} else {
				plaintext->cd_length = saved_length;
			}
			break;
		case AES_GCM_MECH_INFO_TYPE:
		case AES_GMAC_MECH_INFO_TYPE:
			ret = gcm_decrypt_final((gcm_ctx_t *)&aes_ctx,
			    plaintext, AES_BLOCK_LEN, aes_encrypt_block,
			    aes_xor_block);
			ASSERT3U(aes_ctx.ac_remainder_len, ==, 0);
			if ((ret == CRYPTO_SUCCESS) &&
			    (ciphertext != plaintext)) {
				plaintext->cd_length =
				    plaintext->cd_offset - saved_offset;
			} else {
				plaintext->cd_length = saved_length;
			}
			break;
		case AES_CTR_MECH_INFO_TYPE:
			if (ciphertext != plaintext) {
				plaintext->cd_length =
				    plaintext->cd_offset - saved_offset;
			}
			break;
		default:
			ASSERT3U(aes_ctx.ac_remainder_len, ==, 0);
			if (ciphertext != plaintext) {
				plaintext->cd_length =
				    plaintext->cd_offset - saved_offset;
			}
			break;
		}
	} else {
		plaintext->cd_length = saved_length;
	}
	plaintext->cd_offset = saved_offset;

out:
	if (aes_ctx.ac_flags & PROVIDER_OWNS_KEY_SCHEDULE) {
		bzero(aes_ctx.ac_keysched, aes_ctx.ac_keysched_len);
		kmem_free(aes_ctx.ac_keysched, aes_ctx.ac_keysched_len);
	}

	if (aes_ctx.ac_flags & CCM_MODE) {
		if (aes_ctx.ac_pt_buf != NULL) {
			kmem_free(aes_ctx.ac_pt_buf, aes_ctx.ac_data_len);
		}
	} else if (aes_ctx.ac_flags & (GCM_MODE|GMAC_MODE)) {
		if (((gcm_ctx_t *)&aes_ctx)->gcm_pt_buf != NULL) {
			kmem_free(((gcm_ctx_t *)&aes_ctx)->gcm_pt_buf,
			    ((gcm_ctx_t *)&aes_ctx)->gcm_pt_buf_len);
		}
	}

	return (ret);
}

/*
 * KCF software provider context template entry points.
 */
/* ARGSUSED */
static int
aes_create_ctx_template(crypto_provider_handle_t provider,
    crypto_mechanism_t *mechanism, crypto_key_t *key,
    crypto_spi_ctx_template_t *tmpl, size_t *tmpl_size, crypto_req_handle_t req)
{
	void *keysched;
	size_t size;
	int rv;

	if (mechanism->cm_type != AES_ECB_MECH_INFO_TYPE &&
	    mechanism->cm_type != AES_CBC_MECH_INFO_TYPE &&
	    mechanism->cm_type != AES_CMAC_MECH_INFO_TYPE &&
	    mechanism->cm_type != AES_CTR_MECH_INFO_TYPE &&
	    mechanism->cm_type != AES_CCM_MECH_INFO_TYPE &&
	    mechanism->cm_type != AES_GCM_MECH_INFO_TYPE &&
	    mechanism->cm_type != AES_GMAC_MECH_INFO_TYPE)
		return (CRYPTO_MECHANISM_INVALID);

	if ((keysched = aes_alloc_keysched(&size,
	    crypto_kmflag(req))) == NULL) {
		return (CRYPTO_HOST_MEMORY);
	}

	/*
	 * Initialize key schedule.  Key length information is stored
	 * in the key.
	 */
	if ((rv = init_keysched(key, keysched)) != CRYPTO_SUCCESS) {
		bzero(keysched, size);
		kmem_free(keysched, size);
		return (rv);
	}

	*tmpl = keysched;
	*tmpl_size = size;

	return (CRYPTO_SUCCESS);
}


static int
aes_free_context(crypto_ctx_t *ctx)
{
	aes_ctx_t *aes_ctx = ctx->cc_provider_private;

	if (aes_ctx != NULL) {
		if (aes_ctx->ac_flags & PROVIDER_OWNS_KEY_SCHEDULE) {
			ASSERT(aes_ctx->ac_keysched_len != 0);
			bzero(aes_ctx->ac_keysched, aes_ctx->ac_keysched_len);
			kmem_free(aes_ctx->ac_keysched,
			    aes_ctx->ac_keysched_len);
		}
		crypto_free_mode_ctx(aes_ctx);
		ctx->cc_provider_private = NULL;
	}

	return (CRYPTO_SUCCESS);
}


static int
aes_common_init_ctx(aes_ctx_t *aes_ctx, crypto_spi_ctx_template_t *template,
    crypto_mechanism_t *mechanism, crypto_key_t *key, int kmflag,
    boolean_t is_encrypt_init)
{
	int rv = CRYPTO_SUCCESS;
	void *keysched;
	size_t size;

	if (template == NULL) {
		if ((keysched = aes_alloc_keysched(&size, kmflag)) == NULL)
			return (CRYPTO_HOST_MEMORY);
		/*
		 * Initialize key schedule.
		 * Key length is stored in the key.
		 */
		if ((rv = init_keysched(key, keysched)) != CRYPTO_SUCCESS) {
			kmem_free(keysched, size);
			return (rv);
		}

		aes_ctx->ac_flags |= PROVIDER_OWNS_KEY_SCHEDULE;
		aes_ctx->ac_keysched_len = size;
	} else {
		keysched = template;
	}
	aes_ctx->ac_keysched = keysched;

	switch (mechanism->cm_type) {
	case AES_CBC_MECH_INFO_TYPE:
		rv = cbc_init_ctx((cbc_ctx_t *)aes_ctx, mechanism->cm_param,
		    mechanism->cm_param_len, AES_BLOCK_LEN, aes_copy_block64);
		break;
	case AES_CMAC_MECH_INFO_TYPE:
		rv = cmac_init_ctx((cbc_ctx_t *)aes_ctx, AES_BLOCK_LEN);
		break;
	case AES_CTR_MECH_INFO_TYPE: {
		CK_AES_CTR_PARAMS *pp;

		if (mechanism->cm_param == NULL ||
		    mechanism->cm_param_len != sizeof (CK_AES_CTR_PARAMS)) {
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		}
		pp = (CK_AES_CTR_PARAMS *)(void *)mechanism->cm_param;
		rv = ctr_init_ctx((ctr_ctx_t *)aes_ctx, pp->ulCounterBits,
		    pp->cb, aes_encrypt_block, aes_copy_block);
		break;
	}
	case AES_CCM_MECH_INFO_TYPE:
		if (mechanism->cm_param == NULL ||
		    mechanism->cm_param_len != sizeof (CK_AES_CCM_PARAMS)) {
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		}
		rv = ccm_init_ctx((ccm_ctx_t *)aes_ctx, mechanism->cm_param,
		    kmflag, is_encrypt_init, AES_BLOCK_LEN, aes_encrypt_block,
		    aes_xor_block);
		break;
	case AES_GCM_MECH_INFO_TYPE:
		if (mechanism->cm_param == NULL ||
		    mechanism->cm_param_len != sizeof (CK_AES_GCM_PARAMS)) {
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		}
		rv = gcm_init_ctx((gcm_ctx_t *)aes_ctx, mechanism->cm_param,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
		break;
	case AES_GMAC_MECH_INFO_TYPE:
		if (mechanism->cm_param == NULL ||
		    mechanism->cm_param_len != sizeof (CK_AES_GMAC_PARAMS)) {
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		}
		rv = gmac_init_ctx((gcm_ctx_t *)aes_ctx, mechanism->cm_param,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
		break;
	case AES_ECB_MECH_INFO_TYPE:
		aes_ctx->ac_flags |= ECB_MODE;
	}

	if (rv != CRYPTO_SUCCESS) {
		if (aes_ctx->ac_flags & PROVIDER_OWNS_KEY_SCHEDULE) {
			bzero(keysched, size);
			kmem_free(keysched, size);
		}
	}

	return (rv);
}

static int
process_gmac_mech(crypto_mechanism_t *mech, crypto_data_t *data,
    CK_AES_GCM_PARAMS *gcm_params)
{
	/* LINTED: pointer alignment */
	CK_AES_GMAC_PARAMS *params = (CK_AES_GMAC_PARAMS *)mech->cm_param;

	if (mech->cm_type != AES_GMAC_MECH_INFO_TYPE)
		return (CRYPTO_MECHANISM_INVALID);

	if (mech->cm_param_len != sizeof (CK_AES_GMAC_PARAMS))
		return (CRYPTO_MECHANISM_PARAM_INVALID);

	if (params->pIv == NULL)
		return (CRYPTO_MECHANISM_PARAM_INVALID);

	gcm_params->pIv = params->pIv;
	gcm_params->ulIvLen = AES_GMAC_IV_LEN;
	gcm_params->ulTagBits = AES_GMAC_TAG_BITS;

	if (data == NULL)
		return (CRYPTO_SUCCESS);

	if (data->cd_format != CRYPTO_DATA_RAW)
		return (CRYPTO_ARGUMENTS_BAD);

	gcm_params->pAAD = (uchar_t *)data->cd_raw.iov_base;
	gcm_params->ulAADLen = data->cd_length;
	return (CRYPTO_SUCCESS);
}

static int
aes_mac_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t template,
    crypto_req_handle_t req)
{
	return (aes_encrypt_init(ctx, mechanism,
	    key, template, req));
}

static int
aes_mac(crypto_ctx_t *ctx, crypto_data_t *plaintext, crypto_data_t *ciphertext,
    crypto_req_handle_t req)
{
	return (aes_encrypt(ctx, plaintext, ciphertext, req));
}

static int
aes_mac_update(crypto_ctx_t *ctx, crypto_data_t *data,
    crypto_req_handle_t req)
{
	crypto_data_t out;
	uint8_t block[AES_BLOCK_LEN];
	out.cd_format = CRYPTO_DATA_RAW;
	out.cd_offset = 0;
	out.cd_length = sizeof (block);
	out.cd_miscdata = NULL;
	out.cd_raw.iov_base = (void *)block;
	out.cd_raw.iov_len = sizeof (block);

	return (aes_encrypt_update(ctx, data, &out, req));
}

static int
aes_mac_final(crypto_ctx_t *ctx, crypto_data_t *mac, crypto_req_handle_t req)
{
	return (aes_encrypt_final(ctx, mac, req));
}

static int
aes_mac_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *data, crypto_data_t *mac,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	CK_AES_GCM_PARAMS gcm_params;
	crypto_mechanism_t gcm_mech;
	int rv;

	if (mechanism->cm_type == AES_GMAC_MECH_INFO_TYPE) {
		if ((rv = process_gmac_mech(mechanism, data, &gcm_params))
		    != CRYPTO_SUCCESS)
			return (rv);

		gcm_mech.cm_type = AES_GCM_MECH_INFO_TYPE;
		gcm_mech.cm_param_len = sizeof (CK_AES_GCM_PARAMS);
		gcm_mech.cm_param = (char *)&gcm_params;

		return (aes_encrypt_atomic(provider, session_id, &gcm_mech,
		    key, &null_crypto_data, mac, template, req));
	}
	/* CMAC */
	return (aes_encrypt_atomic(provider, session_id, mechanism,
	    key, data, mac, template, req));
}

static int
aes_mac_verify_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *data, crypto_data_t *mac,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	CK_AES_GCM_PARAMS gcm_params;
	crypto_mechanism_t gcm_mech;
	crypto_data_t data_mac;
	char buf[AES_BLOCK_LEN];
	int rv;

	if (mechanism->cm_type == AES_GMAC_MECH_INFO_TYPE) {
		if ((rv = process_gmac_mech(mechanism, data, &gcm_params))
		    != CRYPTO_SUCCESS)
			return (rv);

		gcm_mech.cm_type = AES_GCM_MECH_INFO_TYPE;
		gcm_mech.cm_param_len = sizeof (CK_AES_GCM_PARAMS);
		gcm_mech.cm_param = (char *)&gcm_params;

		return (aes_decrypt_atomic(provider, session_id, &gcm_mech,
		    key, mac, &null_crypto_data, template, req));
	}

	/* CMAC */

	data_mac.cd_format = CRYPTO_DATA_RAW;
	data_mac.cd_offset = 0;
	data_mac.cd_length = AES_BLOCK_LEN;
	data_mac.cd_miscdata = NULL;
	data_mac.cd_raw.iov_base = (void *) buf;
	data_mac.cd_raw.iov_len = AES_BLOCK_LEN;

	rv = aes_encrypt_atomic(provider, session_id, &gcm_mech,
	    key, data, &data_mac, template, req);

	if (rv != CRYPTO_SUCCESS)
		return (rv);

	/* should use get_input_data for mac? */
	if (bcmp(buf, mac->cd_raw.iov_base + mac->cd_offset,
	    AES_BLOCK_LEN) != 0)
		return (CRYPTO_INVALID_MAC);

	return (CRYPTO_SUCCESS);
}
