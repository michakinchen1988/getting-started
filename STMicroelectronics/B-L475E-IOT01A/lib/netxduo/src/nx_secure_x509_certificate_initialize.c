/**************************************************************************/
/*                                                                        */
/*            Copyright (c) 1996-2019 by Express Logic Inc.               */
/*                                                                        */
/*  This software is copyrighted by and is the sole property of Express   */
/*  Logic, Inc.  All rights, title, ownership, or other interests         */
/*  in the software remain the property of Express Logic, Inc.  This      */
/*  software may only be used in accordance with the corresponding        */
/*  license agreement.  Any unauthorized use, duplication, transmission,  */
/*  distribution, or disclosure of this software is expressly forbidden.  */
/*                                                                        */
/*  This Copyright notice may not be removed or modified without prior    */
/*  written consent of Express Logic, Inc.                                */
/*                                                                        */
/*  Express Logic, Inc. reserves the right to modify this software        */
/*  without notice.                                                       */
/*                                                                        */
/*  Express Logic, Inc.                     info@expresslogic.com         */
/*  11423 West Bernardo Court               http://www.expresslogic.com   */
/*  San Diego, CA  92127                                                  */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** NetX Secure Component                                                 */
/**                                                                       */
/**    X509 Digital Certificates                                          */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define NX_SECURE_SOURCE_CODE

#include "nx_secure_tls.h"
#include "nx_secure_x509.h"

/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _nx_secure_x509_certificate_initialize              PORTABLE C      */
/*                                                           5.12         */
/*  AUTHOR                                                                */
/*                                                                        */
/*    Timothy Stapko, Express Logic, Inc.                                 */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*      This function initializes an NX_SECURE_X509_CERTI                 */
/*      structure with a DER-encoded X509 digital certificate, and        */
/*      in the case of a server or client local certificate, the          */
/*      associated private key.                                           */
/*                                                                        */
/*      This function takes a raw data buffer as optional input. The      */
/*      buffer is used to hold the un-parsed certificate data in DER      */
/*      encoded format. If the raw_data_buffer parameter is NX_NULL,      */
/*      The certificate data is referenced directly - DO NOT change       */
/*      the certificate data after calling this function if a separate    */
/*      buffer is not used or unexpected behavior may occur.              */
/*                                                                        */
/*      The private key is also optional. Some certificates (such as      */
/*      in the trusted store) will not have a private key. For such       */
/*      certificates the private key parameter should be passed as        */
/*      NX_NULL.                                                          */
/*                                                                        */
/*      The private key, if supplied, must have a private key type, which */
/*      is defined as a 32-bit value. If the top 16 bits are non-zero,    */
/*      the value is considered user-defined and NetX Secure will perform */
/*      no processing on the key data. If the top 16 bits are zero, the   */
/*      value defines a type known to NetX Secure that will be parsed     */
/*      accordingly. Unknown types in the known-value range will result   */
/*      in an error. No error checking will be performed on user-defined  */
/*      types.                                                            */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    certificate                           Certificate structure         */
/*    certificate_data                      Pointer to certificate data   */
/*    length                                Length of certificate data    */
/*    raw_data_buffer                       Buffer to hold raw cert data  */
/*    buffer_size                           Size of raw data buffer       */
/*    private_key                           Pointer to private key data   */
/*    priv_len                              Length of private key data    */
/*    private_key_type                      Type of private key data      */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    status                                Completion status             */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    _nx_secure_x509_certificate_parse     Extract public key data       */
/*    _nx_secure_x509_pkcs1_rsa_private_key_parse                         */
/*                                          Parse RSA key (PKCS#1 format) */
/*    tx_mutex_get                          Get protection mutex          */
/*    tx_mutex_put                          Put protection mutex          */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    Application Code                                                    */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  06-09-2017     Timothy Stapko           Initial Version 5.10          */
/*  12-15-2017     Timothy Stapko           Modified comment(s), added    */
/*                                            logic to support vendor-    */
/*                                            defined private key type,   */
/*                                            resulting in version 5.11   */
/*  08-15-2019     Timothy Stapko           Modified comment(s), and      */
/*                                            added flexibility of using  */
/*                                            macros instead of direct C  */
/*                                            library function calls,     */
/*                                            released mutex when return, */
/*                                            resulting in version 5.12   */
/*                                                                        */
/**************************************************************************/
UINT _nx_secure_x509_certificate_initialize(NX_SECURE_X509_CERT *certificate,
                                            UCHAR *certificate_data, USHORT length,
                                            UCHAR *raw_data_buffer, USHORT buffer_size,
                                            const UCHAR *private_key, USHORT priv_len,
                                            UINT private_key_type)

{
UINT status;
UINT bytes_processed;

    NX_SECURE_X509_CERTIFICATE_INITIALIZE_EXTENSION

    /* Get the protection. */
    tx_mutex_get(&_nx_secure_tls_protection, TX_WAIT_FOREVER);

    NX_SECURE_MEMSET(certificate, 0, sizeof(NX_SECURE_X509_CERT));

    /* Set up the certificate with raw data. */
    certificate -> nx_secure_x509_certificate_raw_data_length = length;
    if (raw_data_buffer == NX_NULL)
    {
        /* No buffer was passed in so just point to the certificate itself. */
        certificate -> nx_secure_x509_certificate_raw_buffer_size = length;
        certificate -> nx_secure_x509_certificate_raw_data = certificate_data;
    }
    else
    {
        /* Make sure we have enough space in the buffer for the certificate. */
        if (length > buffer_size)
        {
            /* Release the protection. */
            tx_mutex_put(&_nx_secure_tls_protection);
            return(NX_SECURE_TLS_INSUFFICIENT_CERT_SPACE);
        }
        /* Use the caller-supplied buffer for the certificate. */
        certificate -> nx_secure_x509_certificate_raw_buffer_size = buffer_size;
        certificate -> nx_secure_x509_certificate_raw_data = raw_data_buffer;
        NX_SECURE_MEMCPY(certificate -> nx_secure_x509_certificate_raw_data, certificate_data, length);
    }

    /* Parse the DER-encoded X509 certificate to extract the public key data.
     * NOTE: All the pointers returned in the X509 cert will point into the certificate data
     *       passed in here, so DO NOT modify the certificate data or pass in a pointer to a
     *       temporary buffer!*/

    status = _nx_secure_x509_certificate_parse(certificate -> nx_secure_x509_certificate_raw_data,
                                               length, &bytes_processed, certificate);

    if (status != 0)
    {
        /* Release the protection. */
        tx_mutex_put(&_nx_secure_tls_protection);
        return(NX_SECURE_TLS_INVALID_CERTIFICATE);
    }

    /* If the optional private key is supplied, save it for later use. */
    if (private_key != NULL && priv_len > 0)
    {
        /* Save the key type for later. */
        certificate -> nx_secure_x509_private_key_type = private_key_type;

        /* Check for user-defined key types. */
        if ((private_key_type & NX_SECURE_X509_KEY_TYPE_USER_DEFINED_MASK) != 0x0)
        {
            /* User-defined, just save off the key data. */
            certificate -> nx_secure_x509_private_key.user_key.key_data = private_key;
            certificate -> nx_secure_x509_private_key.user_key.key_length = priv_len;
        }
        else
        {
            /* Built-in key type. Attempt to parse the key data. */
            switch (private_key_type)
            {
            case NX_SECURE_X509_KEY_TYPE_RSA_PKCS1_DER:
                status = _nx_secure_x509_pkcs1_rsa_private_key_parse(private_key, priv_len, &bytes_processed, &certificate -> nx_secure_x509_private_key.rsa_private_key);
                break;
            case NX_SECURE_X509_KEY_TYPE_NONE:
            default:
                /* Unknown or invalid key type, return error. */
                status = NX_SECURE_X509_INVALID_PRIVATE_KEY_TYPE;
                break;
            }

            /* See if we had any issues in parsing. */
            if (status != 0)
            {
                /* Release the protection. */
                tx_mutex_put(&_nx_secure_tls_protection);
                return(status);
            }
        }

        /* We have a private key, this is a server or client identity certificate. */
        certificate -> nx_secure_x509_certificate_is_identity_cert = NX_TRUE;
    }
    else
    {
        /* No private key? Cannot be an identity certificate. */
        certificate -> nx_secure_x509_certificate_is_identity_cert = NX_FALSE;
    }

    certificate -> nx_secure_x509_next_certificate = NULL;

    /* Release the protection. */
    tx_mutex_put(&_nx_secure_tls_protection);
    return(NX_SUCCESS);
}

