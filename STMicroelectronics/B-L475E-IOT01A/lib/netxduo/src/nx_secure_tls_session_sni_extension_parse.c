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
/**    Transport Layer Security (TLS)                                     */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define NX_SECURE_SOURCE_CODE

#include "nx_secure_tls.h"

/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _nx_secure_tls_session_sni_extension_parse          PORTABLE C      */
/*                                                           5.12         */
/*  AUTHOR                                                                */
/*                                                                        */
/*    Timothy Stapko, Express Logic, Inc.                                 */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function parses an incoming Hello extensions block, looking    */
/*    for a Server Name Indication (SNI) extension. The SNI extension     */
/*    (RFC 6066) currently contains only a single DNS Name entry, so only */
/*    a single DNS Name is returned. If new RFCs add other name types in  */
/*    the future, new API will be added.                                  */
/*                                                                        */
/*    This function is intended to be called in a TLS Server callback     */
/*    as part of processing Hello message extensions. The SNI extension   */
/*    is only sent by TLS Clients so if a Client receives an SNI          */
/*    extension it should be ignored.                                     */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    tls_session                           TLS control block             */
/*    extensions                            Pointer to received extensions*/
/*    num_extensions                        The number of extensions      */
/*                                            received                    */
/*    dns_name                              Return the name requested by  */
/*                                            the client                  */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    status                                Completion status             */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    Application Code                                                    */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  12-15-2017     Timothy Stapko           Initial Version 5.11          */
/*  08-15-2019     Timothy Stapko           Modified comment(s), and      */
/*                                            added flexibility of using  */
/*                                            macros instead of direct C  */
/*                                            library function calls,     */
/*                                            resulting in version 5.12   */
/*                                                                        */
/**************************************************************************/
UINT _nx_secure_tls_session_sni_extension_parse(NX_SECURE_TLS_SESSION *tls_session,
                                                NX_SECURE_TLS_HELLO_EXTENSION *extensions,
                                                UINT num_extensions, NX_SECURE_X509_DNS_NAME *dns_name)
{
UINT         i;
const UCHAR *data_ptr;
UCHAR        name_type;
USHORT       list_length;
UINT         offset;

    NX_PARAMETER_NOT_USED(tls_session);

    /* Loop through the received extensions until we find SNI or hit the end. */
    for (i = 0; i < num_extensions; ++i)
    {
        if (extensions[i].nx_secure_tls_extension_id == NX_SECURE_TLS_EXTENSION_SERVER_NAME_INDICATION)
        {
            /* Extract the data from the extension. */
            /* Server Name Indication Extension structure:
             * |     2      |      2        |      1     |     2       |   <name length>   |
             * |  Ext Type  |  list length  |  name type | name length |  Host name string |
             */

            /* The generic extension parsing already handled the extension type, so this points to the
               list length field. */
            data_ptr = extensions[i].nx_secure_tls_extension_data;

            /* Extract the list length. */
            list_length = (USHORT)((data_ptr[0] << 8) + data_ptr[1]);
            offset = 2;

            /* Extract the name type. */
            name_type = data_ptr[offset];
            offset += 1;

            /* Extract the name length. */
            dns_name -> nx_secure_x509_dns_name_length = (USHORT)((data_ptr[offset] << 8) + data_ptr[offset + 1]);
            offset += 2;

            /* Check the name type and lengths. */
            if (name_type != NX_SECURE_TLS_SNI_NAME_TYPE_DNS ||
                list_length > extensions[i].nx_secure_tls_extension_data_length ||
                dns_name -> nx_secure_x509_dns_name_length > list_length)
            {
                return(NX_SECURE_TLS_SNI_EXTENSION_INVALID);
            }

            /* Make sure we don't copy over the end of the buffer. */
            if (dns_name -> nx_secure_x509_dns_name_length > NX_SECURE_X509_DNS_NAME_MAX)
            {
                dns_name -> nx_secure_x509_dns_name_length = NX_SECURE_X509_DNS_NAME_MAX;
            }

            /* Name and lengths check out, save off the name data. */
            NX_SECURE_MEMCPY(dns_name -> nx_secure_x509_dns_name, &data_ptr[offset], dns_name -> nx_secure_x509_dns_name_length);

            /* Success! */
            return(NX_SUCCESS);
        }
    }

    /* Searched through all the extensions, did not find SNI. */
    return(NX_SECURE_TLS_EXTENSION_NOT_FOUND);
}

