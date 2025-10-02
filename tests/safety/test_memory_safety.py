#!/usr/bin/env python3
"""
Memory Safety Testing Framework for RazorFS
Tests for buffer overflows, memory leaks, and other memory-related vulnerabilities
"""

import os
import sys
import subprocess
import tempfile
import shutil
import re
import json
from pathlib import Path

class MemorySafetyTester:
    def __init__(self):
        self.test_dir = Path(__file__).parent
        self.repo_root = self.test_dir.parent.parent
        self.results = {
            'total_tests': 0,
            'passed_tests': 0,
            'failed_tests': 0,
            'tests': []
        }
        
    def log(self, level, message):
        """Log messages with color coding"""
        colors = {
            'INFO': '\033[0;34m',
            'SUCCESS': '\033[0;32m',
            'WARNING': '\033[1;33m',
            'ERROR': '\033[0;31m',
            'RESET': '\033[0m'
        }
        
        color = colors.get(level, colors['RESET'])
        print(f"{color}[{level}] {message}{colors['RESET']}")
        
    def run_command(self, cmd, cwd=None, timeout=30):
        """Run a command and capture output"""
        try:
            result = subprocess.run(
                cmd, 
                shell=True, 
                cwd=cwd,
                capture_output=True, 
                text=True, 
                timeout=timeout
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return -1, "", "Command timed out"
        except Exception as e:
            return -1, "", str(e)
            
    def compile_with_asan(self, source_file, output_file):
        """Compile C code with AddressSanitizer"""
        compile_cmd = (
            f"gcc -fsanitize=address -fno-omit-frame-pointer "
            f"-g -Wall -Wextra -std=c11 "
            f"-I{self.test_dir.parent}/unit/kernel "
            f"{source_file} "
            f"{self.test_dir.parent}/unit/kernel/test_framework.c "
            f"-o {output_file}"
        )
        
        returncode, stdout, stderr = self.run_command(compile_cmd)
        return returncode == 0, stdout, stderr
        
    def create_buffer_overflow_test(self):
        """Create a test that should trigger buffer overflow detection"""
        test_code = '''
#include "test_framework.h"

static int test_buffer_overflow(void) {
    char buffer[10];
    
    /* This should trigger AddressSanitizer */
    strcpy(buffer, "This string is definitely longer than 10 characters!");
    
    return 0;
}

static int test_safe_string_copy(void) {
    char buffer[20];
    const char *safe_string = "Safe";
    
    strncpy(buffer, safe_string, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\\0';
    
    TEST_ASSERT_STR_EQ("Safe", buffer, "Safe string copy should work");
    return 0;
}

static struct test_case buffer_tests[] = {
    {"Safe String Copy", test_safe_string_copy},
    {"Buffer Overflow (Should Fail)", test_buffer_overflow},
};

int main(void) {
    return run_test_suite("Buffer Tests", buffer_tests, 2);
}
'''
        return test_code
        
    def create_memory_leak_test(self):
        """Create a test that should trigger memory leak detection"""
        test_code = '''
#include "test_framework.h"

static int test_memory_leak(void) {
    /* Allocate memory but don't free it - should be detected */
    char *ptr = test_malloc(1024);
    TEST_ASSERT_NOT_NULL(ptr, "Memory allocation should succeed");
    
    /* Intentionally not freeing ptr to test leak detection */
    return 0;
}

static int test_proper_cleanup(void) {
    char *ptr = test_malloc(512);
    TEST_ASSERT_NOT_NULL(ptr, "Memory allocation should succeed");
    
    /* Properly free the memory */
    test_free(ptr);
    
    return 0;
}

static int test_double_free(void) {
    char *ptr = test_malloc(256);
    TEST_ASSERT_NOT_NULL(ptr, "Memory allocation should succeed");
    
    test_free(ptr);
    /* This should be safe with our test_free implementation */
    test_free(ptr);
    
    return 0;
}

static struct test_case memory_tests[] = {
    {"Proper Cleanup", test_proper_cleanup},
    {"Double Free Safety", test_double_free},
    {"Memory Leak (Should Fail)", test_memory_leak},
};

int main(void) {
    return run_test_suite("Memory Tests", memory_tests, 3);
}
'''
        return test_code
        
    def create_null_pointer_test(self):
        """Create tests for null pointer handling"""
        test_code = '''
#include "test_framework.h"

static int test_null_pointer_check(void) {
    char *null_ptr = NULL;
    
    /* Our functions should handle NULL pointers safely */
    test_free(null_ptr);  /* Should be safe */
    
    /* Test string operations with NULL */
    if (null_ptr != NULL) {
        strcpy(null_ptr, "test");  /* This would crash, but we check first */
    }
    
    return 0;
}

static int test_null_string_handling(void) {
    const char *null_string = NULL;
    const char *valid_string = "test";
    char buffer[20];
    
    /* Safe string operations */
    if (null_string != NULL) {
        strncpy(buffer, null_string, sizeof(buffer) - 1);
    } else {
        strncpy(buffer, "default", sizeof(buffer) - 1);
    }
    buffer[sizeof(buffer) - 1] = '\\0';
    
    TEST_ASSERT_STR_EQ("default", buffer, "Should use default for NULL string");
    
    return 0;
}

static struct test_case null_tests[] = {
    {"NULL Pointer Check", test_null_pointer_check},
    {"NULL String Handling", test_null_string_handling},
};

int main(void) {
    return run_test_suite("NULL Pointer Tests", null_tests, 2);
}
'''
        return test_code
        
    def test_buffer_overflow_detection(self):
        """Test AddressSanitizer buffer overflow detection"""
        self.log("INFO", "Testing buffer overflow detection...")
        
        with tempfile.TemporaryDirectory() as tmpdir:
            source_file = os.path.join(tmpdir, "buffer_test.c")
            binary_file = os.path.join(tmpdir, "buffer_test")
            
            # Write test code
            with open(source_file, 'w') as f:
                f.write(self.create_buffer_overflow_test())
                
            # Compile with AddressSanitizer
            success, stdout, stderr = self.compile_with_asan(source_file, binary_file)
            
            if not success:
                self.log("ERROR", f"Compilation failed: {stderr}")
                return False
                
            # Run the test - should detect buffer overflow
            returncode, stdout, stderr = self.run_command(binary_file)
            
            # AddressSanitizer should catch the buffer overflow
            asan_detected = "AddressSanitizer" in stderr or "buffer-overflow" in stderr
            
            if asan_detected:
                self.log("SUCCESS", "Buffer overflow detected by AddressSanitizer")
                return True
            else:
                self.log("WARNING", "Buffer overflow not detected - AddressSanitizer may not be working")
                return False
                
    def test_memory_leak_detection(self):
        """Test memory leak detection"""
        self.log("INFO", "Testing memory leak detection...")
        
        with tempfile.TemporaryDirectory() as tmpdir:
            source_file = os.path.join(tmpdir, "leak_test.c")
            binary_file = os.path.join(tmpdir, "leak_test")
            
            # Write test code
            with open(source_file, 'w') as f:
                f.write(self.create_memory_leak_test())
                
            # Compile with AddressSanitizer
            success, stdout, stderr = self.compile_with_asan(source_file, binary_file)
            
            if not success:
                self.log("ERROR", f"Compilation failed: {stderr}")
                return False
                
            # Run the test
            returncode, stdout, stderr = self.run_command(binary_file)
            
            # Check for memory leak detection
            leak_detected = "LeakSanitizer" in stderr or "Memory leak" in stdout
            
            if leak_detected:
                self.log("SUCCESS", "Memory leak detected")
                return True
            else:
                self.log("INFO", "Using custom memory tracking for leak detection")
                # Our test framework should detect the leak
                return "Memory leak detected" in stdout
                
    def test_null_pointer_safety(self):
        """Test null pointer handling"""
        self.log("INFO", "Testing NULL pointer safety...")
        
        with tempfile.TemporaryDirectory() as tmpdir:
            source_file = os.path.join(tmpdir, "null_test.c")
            binary_file = os.path.join(tmpdir, "null_test")
            
            # Write test code
            with open(source_file, 'w') as f:
                f.write(self.create_null_pointer_test())
                
            # Compile with AddressSanitizer
            success, stdout, stderr = self.compile_with_asan(source_file, binary_file)
            
            if not success:
                self.log("ERROR", f"Compilation failed: {stderr}")
                return False
                
            # Run the test
            returncode, stdout, stderr = self.run_command(binary_file)
            
            # Should complete without crashes
            success = returncode == 0 and "ALL TESTS PASSED" in stdout
            
            if success:
                self.log("SUCCESS", "NULL pointer tests passed")
            else:
                self.log("ERROR", f"NULL pointer tests failed: {stderr}")
                
            return success
            
    def test_valgrind_integration(self):
        """Test Valgrind integration if available"""
        self.log("INFO", "Testing Valgrind integration...")
        
        # Check if Valgrind is available
        returncode, _, _ = self.run_command("which valgrind")
        if returncode != 0:
            self.log("WARNING", "Valgrind not available, skipping...")
            return True
            
        with tempfile.TemporaryDirectory() as tmpdir:
            source_file = os.path.join(tmpdir, "valgrind_test.c")
            binary_file = os.path.join(tmpdir, "valgrind_test")
            
            # Simple test program
            test_code = '''
#include "test_framework.h"

int main(void) {
    char *ptr = test_malloc(100);
    test_free(ptr);
    return 0;
}
'''
            
            with open(source_file, 'w') as f:
                f.write(test_code)
                
            # Compile without AddressSanitizer for Valgrind
            compile_cmd = (
                f"gcc -g -Wall -Wextra -std=c11 "
                f"-I{self.test_dir.parent}/unit/kernel "
                f"{source_file} "
                f"{self.test_dir.parent}/unit/kernel/test_framework.c "
                f"-o {binary_file}"
            )
            
            returncode, _, stderr = self.run_command(compile_cmd)
            if returncode != 0:
                self.log("ERROR", f"Compilation failed: {stderr}")
                return False
                
            # Run with Valgrind
            valgrind_cmd = f"valgrind --error-exitcode=1 --leak-check=full {binary_file}"
            returncode, stdout, stderr = self.run_command(valgrind_cmd)
            
            success = returncode == 0
            if success:
                self.log("SUCCESS", "Valgrind test passed")
            else:
                self.log("ERROR", f"Valgrind detected issues: {stderr}")
                
            return success
            
    def run_all_tests(self):
        """Run all memory safety tests"""
        self.log("INFO", "Starting Memory Safety Test Suite...")
        
        tests = [
            ("Buffer Overflow Detection", self.test_buffer_overflow_detection),
            ("Memory Leak Detection", self.test_memory_leak_detection),
            ("NULL Pointer Safety", self.test_null_pointer_safety),
            ("Valgrind Integration", self.test_valgrind_integration),
        ]
        
        for test_name, test_func in tests:
            self.results['total_tests'] += 1
            
            try:
                success = test_func()
                test_result = {
                    'name': test_name,
                    'passed': success,
                    'error': None
                }
                
                if success:
                    self.results['passed_tests'] += 1
                else:
                    self.results['failed_tests'] += 1
                    
            except Exception as e:
                self.log("ERROR", f"Test {test_name} crashed: {e}")
                test_result = {
                    'name': test_name,
                    'passed': False,
                    'error': str(e)
                }
                self.results['failed_tests'] += 1
                
            self.results['tests'].append(test_result)
            
    def generate_report(self):
        """Generate test report"""
        self.log("INFO", "Generating memory safety test report...")
        
        report_file = self.test_dir.parent / "results" / "memory_safety_report.json"
        report_file.parent.mkdir(exist_ok=True)
        
        with open(report_file, 'w') as f:
            json.dump(self.results, f, indent=2)
            
        # Print summary
        print(f"\n{'='*50}")
        print(f"Memory Safety Test Summary")
        print(f"{'='*50}")
        print(f"Total Tests: {self.results['total_tests']}")
        print(f"Passed: {self.results['passed_tests']}")
        print(f"Failed: {self.results['failed_tests']}")
        
        if self.results['failed_tests'] == 0:
            self.log("SUCCESS", "ALL MEMORY SAFETY TESTS PASSED!")
        else:
            self.log("ERROR", f"{self.results['failed_tests']} TESTS FAILED!")
            
        print(f"Report saved to: {report_file}")
        
        return self.results['failed_tests'] == 0

def main():
    tester = MemorySafetyTester()
    tester.run_all_tests()
    success = tester.generate_report()
    
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())