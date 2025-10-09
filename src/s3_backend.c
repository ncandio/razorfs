/**
 * S3 Storage Backend Implementation for RAZORFS
 * Provides cloud storage integration with AWS S3-compatible services
 */

#include "s3_backend.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

/* Static error buffer */
static char s3_error_buffer[512] = {0};

/* Set last error message */
static void s3_set_error(const char* msg) {
    if (msg) {
        strncpy(s3_error_buffer, msg, sizeof(s3_error_buffer) - 1);
        s3_error_buffer[sizeof(s3_error_buffer) - 1] = '\0';
    } else {
        s3_error_buffer[0] = '\0';
    }
}

const char* s3_get_last_error(void) {
    return s3_error_buffer[0] ? s3_error_buffer : NULL;
}

int s3_backend_init(struct s3_backend* backend, 
                   const char* bucket_name,
                   const char* region,
                   const char* endpoint) {
    if (!backend || !bucket_name) {
        s3_set_error("Invalid parameters: backend and bucket_name required");
        return -1;
    }

    memset(backend, 0, sizeof(*backend));
    
    /* Copy bucket name */
    strncpy(backend->bucket_name, bucket_name, sizeof(backend->bucket_name) - 1);
    
    /* Set region (use default if not provided) */
    if (region) {
        strncpy(backend->region, region, sizeof(backend->region) - 1);
    } else {
        strncpy(backend->region, S3_DEFAULT_REGION, sizeof(backend->region) - 1);
    }
    
    /* Set endpoint (use default if not provided) */
    if (endpoint) {
        strncpy(backend->endpoint, endpoint, sizeof(backend->endpoint) - 1);
    } else {
        strncpy(backend->endpoint, S3_DEFAULT_ENDPOINT, sizeof(backend->endpoint) - 1);
    }
    
    backend->initialized = true;
    backend->use_ssl = true;
    
    s3_set_error(NULL); /* Clear error */
    return 0;
}

int s3_backend_configure_credentials(struct s3_backend* backend,
                                    const char* access_key,
                                    const char* secret_key) {
    if (!backend || !access_key || !secret_key) {
        s3_set_error("Invalid parameters: all credentials required");
        return -1;
    }
    
    if (!backend->initialized) {
        s3_set_error("Backend not initialized");
        return -1;
    }
    
    /* Copy credentials */
    strncpy(backend->access_key, access_key, sizeof(backend->access_key) - 1);
    strncpy(backend->secret_key, secret_key, sizeof(backend->secret_key) - 1);
    
    s3_set_error(NULL); /* Clear error */
    return 0;
}

#ifdef HAS_AWS_SDK

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <iostream>
#include <fstream>
#include <sstream>

/* AWS SDK initialization guard */
static bool aws_sdk_initialized = false;

/* Initialize AWS SDK if needed */
static int ensure_aws_sdk_initialized(void) {
    if (!aws_sdk_initialized) {
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        aws_sdk_initialized = true;
    }
    return 0;
}

int s3_upload_object(struct s3_backend* backend,
                    const char* key,
                    const void* data,
                    size_t size,
                    const struct s3_object_metadata* metadata) {
    if (!backend || !key || !data || size == 0) {
        s3_set_error("Invalid parameters");
        return -1;
    }
    
    if (!backend->initialized) {
        s3_set_error("Backend not initialized");
        return -1;
    }
    
    /* Ensure AWS SDK is initialized */
    if (ensure_aws_sdk_initialized() != 0) {
        s3_set_error("Failed to initialize AWS SDK");
        return -1;
    }
    
    try {
        /* Create S3 client if needed */
        if (!backend->s3_client) {
            Aws::Client::ClientConfiguration config;
            config.region = backend->region;
            config.endpointOverride = backend->endpoint;
            
            if (backend->access_key[0] && backend->secret_key[0]) {
                Aws::Auth::AWSCredentials credentials(backend->access_key, backend->secret_key);
                backend->s3_client = new Aws::S3::S3Client(credentials, config);
            } else {
                backend->s3_client = new Aws::S3::S3Client(config);
            }
        }
        
        /* Create put object request */
        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(backend->bucket_name);
        request.SetKey(key);
        
        /* Create stream from data */
        auto data_stream = Aws::MakeShared<Aws::StringStream>("PutObjectStream");
        data_stream->write(static_cast<const char*>(data), size);
        request.SetBody(data_stream);
        
        /* Set metadata if provided */
        if (metadata) {
            Aws::Map<Aws::String, Aws::String> meta_map;
            meta_map["content-type"] = metadata->content_type;
            request.SetMetadata(meta_map);
        }
        
        /* Execute request */
        auto outcome = backend->s3_client->PutObject(request);
        
        if (outcome.IsSuccess()) {
            s3_set_error(NULL);
            return 0;
        } else {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "S3 PutObject failed: %s", 
                     outcome.GetError().GetMessage().c_str());
            s3_set_error(error_msg);
            return -1;
        }
        
    } catch (const std::exception& e) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Exception during S3 upload: %s", e.what());
        s3_set_error(error_msg);
        return -1;
    }
}

int s3_download_object(struct s3_backend* backend,
                       const char* key,
                       void** data_out,
                       size_t* size_out) {
    if (!backend || !key || !data_out || !size_out) {
        s3_set_error("Invalid parameters");
        return -1;
    }
    
    if (!backend->initialized) {
        s3_set_error("Backend not initialized");
        return -1;
    }
    
    /* Ensure AWS SDK is initialized */
    if (ensure_aws_sdk_initialized() != 0) {
        s3_set_error("Failed to initialize AWS SDK");
        return -1;
    }
    
    try {
        /* Create S3 client if needed */
        if (!backend->s3_client) {
            Aws::Client::ClientConfiguration config;
            config.region = backend->region;
            config.endpointOverride = backend->endpoint;
            
            if (backend->access_key[0] && backend->secret_key[0]) {
                Aws::Auth::AWSCredentials credentials(backend->access_key, backend->secret_key);
                backend->s3_client = new Aws::S3::S3Client(credentials, config);
            } else {
                backend->s3_client = new Aws::S3::S3Client(config);
            }
        }
        
        /* Create get object request */
        Aws::S3::Model::GetObjectRequest request;
        request.SetBucket(backend->bucket_name);
        request.SetKey(key);
        
        /* Execute request */
        auto outcome = backend->s3_client->GetObject(request);
        
        if (outcome.IsSuccess()) {
            auto& body = outcome.GetResult().GetBody();
            
            /* Get content length */
            size_t content_length = outcome.GetResult().GetContentLength();
            
            /* Allocate buffer */
            void* buffer = malloc(content_length);
            if (!buffer) {
                s3_set_error("Memory allocation failed");
                return -1;
            }
            
            /* Read data into buffer */
            body.read(static_cast<char*>(buffer), content_length);
            
            *data_out = buffer;
            *size_out = content_length;
            
            s3_set_error(NULL);
            return 0;
        } else {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "S3 GetObject failed: %s", 
                     outcome.GetError().GetMessage().c_str());
            s3_set_error(error_msg);
            return -1;
        }
        
    } catch (const std::exception& e) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Exception during S3 download: %s", e.what());
        s3_set_error(error_msg);
        return -1;
    }
}

int s3_delete_object(struct s3_backend* backend,
                     const char* key) {
    if (!backend || !key) {
        s3_set_error("Invalid parameters");
        return -1;
    }
    
    if (!backend->initialized) {
        s3_set_error("Backend not initialized");
        return -1;
    }
    
    /* Ensure AWS SDK is initialized */
    if (ensure_aws_sdk_initialized() != 0) {
        s3_set_error("Failed to initialize AWS SDK");
        return -1;
    }
    
    try {
        /* Create S3 client if needed */
        if (!backend->s3_client) {
            Aws::Client::ClientConfiguration config;
            config.region = backend->region;
            config.endpointOverride = backend->endpoint;
            
            if (backend->access_key[0] && backend->secret_key[0]) {
                Aws::Auth::AWSCredentials credentials(backend->access_key, backend->secret_key);
                backend->s3_client = new Aws::S3::S3Client(credentials, config);
            } else {
                backend->s3_client = new Aws::S3::S3Client(config);
            }
        }
        
        /* Create delete object request */
        Aws::S3::Model::DeleteObjectRequest request;
        request.SetBucket(backend->bucket_name);
        request.SetKey(key);
        
        /* Execute request */
        auto outcome = backend->s3_client->DeleteObject(request);
        
        if (outcome.IsSuccess()) {
            s3_set_error(NULL);
            return 0;
        } else {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "S3 DeleteObject failed: %s", 
                     outcome.GetError().GetMessage().c_str());
            s3_set_error(error_msg);
            return -1;
        }
        
    } catch (const std::exception& e) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Exception during S3 delete: %s", e.what());
        s3_set_error(error_msg);
        return -1;
    }
}

int s3_get_object_metadata(struct s3_backend* backend,
                          const char* key,
                          struct s3_object_metadata* metadata_out) {
    if (!backend || !key || !metadata_out) {
        s3_set_error("Invalid parameters");
        return -1;
    }
    
    if (!backend->initialized) {
        s3_set_error("Backend not initialized");
        return -1;
    }
    
    /* Ensure AWS SDK is initialized */
    if (ensure_aws_sdk_initialized() != 0) {
        s3_set_error("Failed to initialize AWS SDK");
        return -1;
    }
    
    try {
        /* Create S3 client if needed */
        if (!backend->s3_client) {
            Aws::Client::ClientConfiguration config;
            config.region = backend->region;
            config.endpointOverride = backend->endpoint;
            
            if (backend->access_key[0] && backend->secret_key[0]) {
                Aws::Auth::AWSCredentials credentials(backend->access_key, backend->secret_key);
                backend->s3_client = new Aws::S3::S3Client(credentials, config);
            } else {
                backend->s3_client = new Aws::S3::S3Client(config);
            }
        }
        
        /* Create head object request */
        Aws::S3::Model::HeadObjectRequest request;
        request.SetBucket(backend->bucket_name);
        request.SetKey(key);
        
        /* Execute request */
        auto outcome = backend->s3_client->HeadObject(request);
        
        if (outcome.IsSuccess()) {
            const auto& result = outcome.GetResult();
            
            /* Fill metadata */
            strncpy(metadata_out->key, key, sizeof(metadata_out->key) - 1);
            metadata_out->size = result.GetContentLength();
            metadata_out->last_modified = result.GetLastModified().Seconds();
            
            /* Copy ETag if available */
            const auto& etag = result.GetETag();
            if (!etag.empty()) {
                strncpy(metadata_out->etag, etag.c_str(), sizeof(metadata_out->etag) - 1);
            }
            
            /* Copy content type if available */
            const auto& content_type = result.GetContentType();
            if (!content_type.empty()) {
                strncpy(metadata_out->content_type, content_type.c_str(), sizeof(metadata_out->content_type) - 1);
            }
            
            s3_set_error(NULL);
            return 0;
        } else {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "S3 HeadObject failed: %s", 
                     outcome.GetError().GetMessage().c_str());
            s3_set_error(error_msg);
            return -1;
        }
        
    } catch (const std::exception& e) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Exception during S3 metadata retrieval: %s", e.what());
        s3_set_error(error_msg);
        return -1;
    }
}

int s3_object_exists(struct s3_backend* backend,
                    const char* key) {
    if (!backend || !key) {
        s3_set_error("Invalid parameters");
        return -1;
    }
    
    if (!backend->initialized) {
        s3_set_error("Backend not initialized");
        return -1;
    }
    
    /* Ensure AWS SDK is initialized */
    if (ensure_aws_sdk_initialized() != 0) {
        s3_set_error("Failed to initialize AWS SDK");
        return -1;
    }
    
    try {
        /* Create S3 client if needed */
        if (!backend->s3_client) {
            Aws::Client::ClientConfiguration config;
            config.region = backend->region;
            config.endpointOverride = backend->endpoint;
            
            if (backend->access_key[0] && backend->secret_key[0]) {
                Aws::Auth::AWSCredentials credentials(backend->access_key, backend->secret_key);
                backend->s3_client = new Aws::S3::S3Client(credentials, config);
            } else {
                backend->s3_client = new Aws::S3::S3Client(config);
            }
        }
        
        /* Create head object request */
        Aws::S3::Model::HeadObjectRequest request;
        request.SetBucket(backend->bucket_name);
        request.SetKey(key);
        
        /* Execute request */
        auto outcome = backend->s3_client->HeadObject(request);
        
        if (outcome.IsSuccess()) {
            s3_set_error(NULL);
            return 1; /* Object exists */
        } else {
            const auto& error = outcome.GetError();
            if (error.GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY ||
                error.GetErrorType() == Aws::S3::S3Errors::NOT_FOUND) {
                s3_set_error(NULL);
                return 0; /* Object doesn't exist */
            } else {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "S3 HeadObject failed: %s", 
                         error.GetMessage().c_str());
                s3_set_error(error_msg);
                return -1; /* Error occurred */
            }
        }
        
    } catch (const std::exception& e) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Exception during S3 existence check: %s", e.what());
        s3_set_error(error_msg);
        return -1;
    }
}

#else /* HAS_AWS_SDK not defined */

/* Stub implementations when AWS SDK is not available */

int s3_upload_object(struct s3_backend* backend,
                    const char* key,
                    const void* data,
                    size_t size,
                    const struct s3_object_metadata* metadata) {
    s3_set_error("AWS SDK not available - S3 integration disabled");
    return -1;
}

int s3_download_object(struct s3_backend* backend,
                       const char* key,
                       void** data_out,
                       size_t* size_out) {
    s3_set_error("AWS SDK not available - S3 integration disabled");
    return -1;
}

int s3_delete_object(struct s3_backend* backend,
                     const char* key) {
    s3_set_error("AWS SDK not available - S3 integration disabled");
    return -1;
}

int s3_get_object_metadata(struct s3_backend* backend,
                          const char* key,
                          struct s3_object_metadata* metadata_out) {
    s3_set_error("AWS SDK not available - S3 integration disabled");
    return -1;
}

int s3_object_exists(struct s3_backend* backend,
                    const char* key) {
    s3_set_error("AWS SDK not available - S3 integration disabled");
    return -1;
}

#endif /* HAS_AWS_SDK */

void s3_backend_shutdown(struct s3_backend* backend) {
    if (!backend) return;
    
    /* Clean up AWS SDK resources if available */
#ifdef HAS_AWS_SDK
    if (backend->s3_client) {
        delete backend->s3_client;
        backend->s3_client = NULL;
    }
#endif
    
    /* Clear credentials */
    memset(backend->access_key, 0, sizeof(backend->access_key));
    memset(backend->secret_key, 0, sizeof(backend->secret_key));
    
    backend->initialized = false;
}