/**
 * S3 Storage Backend for RAZORFS
 * Provides cloud storage integration with AWS S3-compatible services
 */

#ifndef RAZORFS_S3_BACKEND_H
#define RAZORFS_S3_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef HAS_AWS_SDK
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#endif

/* Configuration */
#define S3_MAX_KEY_LENGTH 1024
#define S3_DEFAULT_REGION "us-east-1"
#define S3_DEFAULT_ENDPOINT "https://s3.amazonaws.com"

/* S3 Backend Context */
struct s3_backend {
    char bucket_name[256];
    char region[64];
    char endpoint[256];
    char access_key[256];
    char secret_key[256];
    
#ifdef HAS_AWS_SDK
    struct Aws::S3::S3Client* s3_client;
#endif
    
    bool initialized;
    bool use_ssl;
};

/* S3 Object Metadata */
struct s3_object_metadata {
    char key[S3_MAX_KEY_LENGTH];
    size_t size;
    time_t last_modified;
    char etag[64];
    char content_type[128];
};

/**
 * Initialize S3 backend
 * @param backend Backend context to initialize
 * @param bucket_name S3 bucket name
 * @param region AWS region (optional, can be NULL for default)
 * @param endpoint Custom endpoint (optional, can be NULL for AWS)
 * @return 0 on success, -1 on failure
 */
int s3_backend_init(struct s3_backend* backend, 
                   const char* bucket_name,
                   const char* region,
                   const char* endpoint);

/**
 * Configure S3 credentials
 * @param backend Backend context
 * @param access_key AWS access key
 * @param secret_key AWS secret key
 * @return 0 on success, -1 on failure
 */
int s3_backend_configure_credentials(struct s3_backend* backend,
                                    const char* access_key,
                                    const char* secret_key);

/**
 * Upload data to S3
 * @param backend Backend context
 * @param key Object key/name
 * @param data Data to upload
 * @param size Size of data in bytes
 * @param metadata Optional metadata (can be NULL)
 * @return 0 on success, -1 on failure
 */
int s3_upload_object(struct s3_backend* backend,
                    const char* key,
                    const void* data,
                    size_t size,
                    const struct s3_object_metadata* metadata);

/**
 * Download data from S3
 * @param backend Backend context
 * @param key Object key/name
 * @param data_out Output buffer (caller must free)
 * @param size_out Size of downloaded data
 * @return 0 on success, -1 on failure
 */
int s3_download_object(struct s3_backend* backend,
                       const char* key,
                       void** data_out,
                       size_t* size_out);

/**
 * Delete object from S3
 * @param backend Backend context
 * @param key Object key/name
 * @return 0 on success, -1 on failure
 */
int s3_delete_object(struct s3_backend* backend,
                     const char* key);

/**
 * Get object metadata
 * @param backend Backend context
 * @param key Object key/name
 * @param metadata_out Metadata structure to fill
 * @return 0 on success, -1 on failure
 */
int s3_get_object_metadata(struct s3_backend* backend,
                          const char* key,
                          struct s3_object_metadata* metadata_out);

/**
 * List objects with prefix
 * @param backend Backend context
 * @param prefix Object key prefix (can be NULL)
 * @param max_keys Maximum number of keys to return (0 for default)
 * @return 0 on success, -1 on failure
 */
int s3_list_objects(struct s3_backend* backend,
                   const char* prefix,
                   uint32_t max_keys);

/**
 * Check if object exists
 * @param backend Backend context
 * @param key Object key/name
 * @return 1 if exists, 0 if not, -1 on error
 */
int s3_object_exists(struct s3_backend* backend,
                    const char* key);

/**
 * Shutdown S3 backend
 * @param backend Backend context
 */
void s3_backend_shutdown(struct s3_backend* backend);

/**
 * Get last error message
 * @return Last error message (do not free)
 */
const char* s3_get_last_error(void);

#endif /* RAZORFS_S3_BACKEND_H */