/**
 * S3 Backend Test Program
 * Tests RAZORFS S3 integration capabilities
 */

#include "src/s3_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <bucket_name> [access_key] [secret_key]\n", argv[0]);
        fprintf(stderr, "Note: For testing without credentials, set AWS credentials via environment or IAM role\n");
        return 1;
    }
    
    const char* bucket_name = argv[1];
    const char* access_key = (argc > 2) ? argv[2] : getenv("AWS_ACCESS_KEY_ID");
    const char* secret_key = (argc > 3) ? argv[3] : getenv("AWS_SECRET_ACCESS_KEY");
    const char* region = getenv("AWS_DEFAULT_REGION") ?: "us-east-1";
    
    printf("🔍 RAZORFS S3 Backend Test\n");
    printf("=========================\n");
    printf("Bucket: %s\n", bucket_name);
    printf("Region: %s\n", region);
    printf("Credentials: %s\n", access_key ? "Provided" : "From environment/IAM");
    
    /* Initialize S3 backend */
    struct s3_backend backend;
    printf("\n🔄 Initializing S3 backend...\n");
    
    if (s3_backend_init(&backend, bucket_name, region, NULL) != 0) {
        fprintf(stderr, "❌ Failed to initialize S3 backend: %s\n", s3_get_last_error());
        return 1;
    }
    
    printf("✅ S3 backend initialized successfully\n");
    
    /* Configure credentials if provided */
    if (access_key && secret_key) {
        printf("🔐 Configuring credentials...\n");
        if (s3_backend_configure_credentials(&backend, access_key, secret_key) != 0) {
            fprintf(stderr, "❌ Failed to configure credentials: %s\n", s3_get_last_error());
            s3_backend_shutdown(&backend);
            return 1;
        }
        printf("✅ Credentials configured successfully\n");
    } else {
        printf("⚠️  No explicit credentials provided, using environment/IAM role\n");
    }
    
    /* Test data */
    const char* test_data = "Hello RAZORFS S3 Integration! This is test data for cloud storage.";
    size_t test_data_size = strlen(test_data);
    const char* test_key = "razorfs_test_object.txt";
    
    printf("\n📤 Testing S3 Upload...\n");
    struct s3_object_metadata metadata = {0};
    strncpy(metadata.content_type, "text/plain", sizeof(metadata.content_type) - 1);
    
    if (s3_upload_object(&backend, test_key, test_data, test_data_size, &metadata) != 0) {
        fprintf(stderr, "❌ Failed to upload test object: %s\n", s3_get_last_error());
        s3_backend_shutdown(&backend);
        return 1;
    }
    
    printf("✅ Uploaded test object '%s' (%zu bytes)\n", test_key, test_data_size);
    
    /* Test object existence */
    printf("\n🔍 Testing Object Existence...\n");
    int exists = s3_object_exists(&backend, test_key);
    if (exists == 1) {
        printf("✅ Object '%s' exists in bucket\n", test_key);
    } else if (exists == 0) {
        printf("❌ Object '%s' does not exist in bucket\n", test_key);
    } else {
        fprintf(stderr, "⚠️  Error checking object existence: %s\n", s3_get_last_error());
    }
    
    /* Get object metadata */
    printf("\n📋 Testing Object Metadata Retrieval...\n");
    struct s3_object_metadata retrieved_metadata = {0};
    if (s3_get_object_metadata(&backend, test_key, &retrieved_metadata) == 0) {
        printf("✅ Retrieved object metadata:\n");
        printf("   Size: %zu bytes\n", retrieved_metadata.size);
        printf("   Content-Type: %s\n", retrieved_metadata.content_type);
        if (retrieved_metadata.etag[0]) {
            printf("   ETag: %s\n", retrieved_metadata.etag);
        }
    } else {
        fprintf(stderr, "❌ Failed to retrieve object metadata: %s\n", s3_get_last_error());
    }
    
    /* Download test */
    printf("\n📥 Testing S3 Download...\n");
    void* downloaded_data = NULL;
    size_t downloaded_size = 0;
    
    if (s3_download_object(&backend, test_key, &downloaded_data, &downloaded_size) == 0) {
        printf("✅ Downloaded object '%s' (%zu bytes)\n", test_key, downloaded_size);
        
        /* Verify data integrity */
        if (downloaded_size == test_data_size && 
            memcmp(downloaded_data, test_data, test_data_size) == 0) {
            printf("✅ Data integrity verified - download matches original\n");
        } else {
            printf("❌ Data integrity check failed!\n");
            printf("   Original size: %zu, Downloaded size: %zu\n", test_data_size, downloaded_size);
        }
        
        free(downloaded_data);
    } else {
        fprintf(stderr, "❌ Failed to download test object: %s\n", s3_get_last_error());
    }
    
    /* Cleanup test object */
    printf("\n🧹 Cleaning up test object...\n");
    if (s3_delete_object(&backend, test_key) == 0) {
        printf("✅ Test object '%s' deleted successfully\n", test_key);
    } else {
        fprintf(stderr, "⚠️  Failed to delete test object: %s\n", s3_get_last_error());
    }
    
    /* Shutdown backend */
    s3_backend_shutdown(&backend);
    printf("\n✅ S3 Backend Test Complete!\n");
    
    return 0;
}