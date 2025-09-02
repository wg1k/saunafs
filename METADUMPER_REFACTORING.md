# SaunaFS Metadata Dumper Service Refactoring

## Overview

This document describes the refactoring of SaunaFS metadata dumping functionality from a fork-based approach to a dedicated service architecture. The refactoring decouples metadata dumping from the master server process, enabling better resource management, scalability, and operational flexibility while maintaining full backward compatibility.

## Architecture Changes

### Before: Fork-Based Approach
- Master server forks a child process for metadata dumping
- Child process executes `sfsmetarestore` directly
- Communication via pipes
- Resource usage tied to master server process

### After: Service-Based Architecture
- Standalone `saunafs-metadumper` service
- Unix domain socket communication
- Asynchronous request/response protocol
- Independent resource management
- Optional fallback to original fork-based approach

## Key Benefits

1. **Decoupled Resource Usage**: Metadata dumping no longer affects master server memory and CPU
2. **Better Scalability**: Service can handle multiple concurrent dump requests
3. **Improved Reliability**: Service failures don't impact master server stability
4. **Operational Flexibility**: Service can be deployed on different machines
5. **Backward Compatibility**: Original fork-based approach remains available
6. **Gradual Migration**: Feature flag allows smooth transition




## New Components

### 1. Metadumper Library (`src/metadumper_lib/`)
- **Purpose**: Reusable library containing core metadata dumping functionality
- **Files**:
  - `metadumper_lib.h` - Library interface
  - `metadumper_lib.cc` - Core dumping logic extracted from metarestore
  - `CMakeLists.txt` - Build configuration

### 2. Metadumper Service (`src/metadumper_service/`)
- **Purpose**: Standalone service for handling metadata dump requests
- **Files**:
  - `metadumper_service.h/.cc` - Main service implementation
  - `protocol.h/.cc` - Communication protocol definitions
  - `file_transfer.h/.cc` - Efficient file transfer mechanisms
  - `main.cc` - Service entry point
  - `CMakeLists.txt` - Build configuration

### 3. Master Integration (`src/master/`)
- **Modified Files**:
  - `metadata_dumper_file.h/.cc` - Updated to support service communication
- **New Files**:
  - `metadumper_client.h/.cc` - Service client implementation
  - `metadumper_config.h/.cc` - Configuration management

### 4. Configuration
- **File**: `etc/metadumper.conf` - Example configuration file
- **Environment Variables**: Support for runtime configuration

## Configuration Options

The refactoring introduces several configuration options to control behavior:

### Primary Control Flag
- `METADUMPER_USE_SERVICE_ARCHITECTURE` (default: `false`)
  - `true`: Enable new service-based architecture
  - `false`: Use original fork-based approach (backward compatible)

### Service-Specific Options (only used when service architecture is enabled)
- `METADUMPER_SERVICE_SOCKET` (default: `/var/run/saunafs/metadumper.sock`)
  - Unix domain socket path for service communication
- `METADUMPER_SERVICE_ENABLED` (default: `true`)
  - Enable/disable service communication attempts
- `METADUMPER_FALLBACK_ENABLED` (default: `true`)
  - Allow fallback to fork-based approach if service fails
- `METADUMPER_SERVICE_TIMEOUT_MS` (default: `30000`)
  - Service communication timeout in milliseconds
- `METADUMPER_MAX_RETRIES` (default: `3`)
  - Maximum retry attempts for service communication


## Build Instructions

### Prerequisites
Before building, ensure the SaunaFS environment is properly set up:

```bash
# Run the SaunaFS setup script (required for proper build and test environment)
./setup-machine.sh setup /mnt/hdd_01
```

### Standard Build Process
```bash
# Install dependencies
sudo apt update
sudo apt install -y cmake build-essential pkg-config libfuse3-dev \
    libboost-all-dev libjudy-dev libpam0g-dev libspdlog-dev \
    libfmt-dev libyaml-cpp-dev

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install
sudo make install
```

### Building Individual Components
```bash
# Build metadumper library
make metadumper_lib

# Build metadumper service
make saunafs-metadumper

# Build master with new integration
make sfsmaster
```

## Deployment Guide

### 1. Backward Compatible Deployment (Default)
No changes required. The system uses the original fork-based approach by default.

### 2. Service-Based Deployment

#### Step 1: Deploy the Service
```bash
# Copy service binary
sudo cp build/src/metadumper_service/saunafs-metadumper /usr/sbin/

# Create service directory
sudo mkdir -p /var/run/saunafs
sudo chown saunafs:saunafs /var/run/saunafs

# Create configuration file
sudo cp etc/metadumper.conf /etc/saunafs/metadumper.conf
```

#### Step 2: Configure the Service
Edit `/etc/saunafs/metadumper.conf`:
```bash
# Enable the new service architecture
METADUMPER_USE_SERVICE_ARCHITECTURE=true

# Configure service socket path
METADUMPER_SERVICE_SOCKET=/var/run/saunafs/metadumper.sock

# Enable service with fallback for safety
METADUMPER_SERVICE_ENABLED=true
METADUMPER_FALLBACK_ENABLED=true
```

#### Step 3: Start the Service
```bash
# Start the metadumper service
sudo systemctl start saunafs-metadumper

# Enable automatic startup
sudo systemctl enable saunafs-metadumper
```

#### Step 4: Configure the Master
Set environment variables for the master process:
```bash
# In master service configuration or environment file
export METADUMPER_USE_SERVICE_ARCHITECTURE=true
export METADUMPER_SERVICE_ENABLED=true
export METADUMPER_FALLBACK_ENABLED=true
export METADUMPER_SERVICE_SOCKET=/var/run/saunafs/metadumper.sock
```

#### Step 5: Restart the Master
```bash
sudo systemctl restart saunafs-master
```


## Migration Strategy

### Phase 1: Preparation
1. Build and deploy the new components alongside existing ones
2. Keep `METADUMPER_USE_SERVICE_ARCHITECTURE=false` (default)
3. Verify that existing functionality remains unchanged

### Phase 2: Service Deployment
1. Deploy the `saunafs-metadumper` service
2. Configure and start the service
3. Keep master using fork-based approach initially

### Phase 3: Gradual Enablement
1. Enable service architecture: `METADUMPER_USE_SERVICE_ARCHITECTURE=true`
2. Keep fallback enabled: `METADUMPER_FALLBACK_ENABLED=true`
3. Monitor logs for service communication
4. Verify metadata dumps are working correctly

### Phase 4: Full Migration (Optional)
1. After confirming service stability, optionally disable fallback:
   `METADUMPER_FALLBACK_ENABLED=false`
2. This ensures exclusive use of the service architecture

### Rollback Plan
At any point, rollback by setting:
```bash
export METADUMPER_USE_SERVICE_ARCHITECTURE=false
```
This immediately reverts to the original fork-based approach.

## Testing Instructions

### Environment Setup
```bash
# Ensure proper SaunaFS environment setup
./setup-machine.sh setup /mnt/hdd_01
```

### Configuration Testing
```bash
# Test configuration system
cd test_metadumper
g++ -o test_config test_config.cc

# Test default behavior (backward compatible)
./test_config

# Test with service architecture enabled
export METADUMPER_USE_SERVICE_ARCHITECTURE=true
export METADUMPER_SERVICE_ENABLED=true
export METADUMPER_FALLBACK_ENABLED=true
./test_config
```

### Integration Testing
1. **Fork-Based Testing** (default):
   ```bash
   # Start master with default configuration
   # Trigger metadata dump
   # Verify dump files are created correctly
   ```

2. **Service-Based Testing**:
   ```bash
   # Start metadumper service
   sudo /usr/sbin/saunafs-metadumper -f
   
   # In another terminal, start master with service configuration
   export METADUMPER_USE_SERVICE_ARCHITECTURE=true
   # Start master and trigger metadata dump
   # Verify service communication and dump creation
   ```

3. **Fallback Testing**:
   ```bash
   # Stop metadumper service while master is running
   # Trigger metadata dump
   # Verify fallback to fork-based approach works
   ```

## Monitoring and Troubleshooting

### Log Messages
The refactoring adds specific log messages to help with monitoring:

- **Service Architecture Enabled**: "Using service-based metadata dumping"
- **Service Communication**: "Sent dump request to service with ID: {id}"
- **Service Success**: "Service metadata dump completed successfully"
- **Fallback Triggered**: "Failed to send dump request to service, falling back to local dump"
- **Fork Fallback**: "Using fallback fork-based metadata dumping"

### Common Issues

1. **Service Not Available**:
   - Check if `saunafs-metadumper` service is running
   - Verify socket path configuration matches
   - Check socket permissions

2. **Permission Issues**:
   - Ensure service runs as appropriate user (typically `saunafs`)
   - Verify socket directory permissions
   - Check metadata file write permissions

3. **Configuration Mismatch**:
   - Verify environment variables are set correctly
   - Check configuration file syntax
   - Ensure master and service use same socket path


## Technical Implementation Details

### Communication Protocol
The service uses a simple binary protocol over Unix domain sockets:

```cpp
struct DumpRequest {
    std::string requestId;        // Unique request identifier
    uint64_t checksum;           // Expected metadata checksum
    std::string changelogFile;   // Path to changelog file
    std::string outputPath;      // Directory for output files
    std::string metadataFile;    // Path to metadata file
    int storedMetaCopies;        // Number of backup copies
};

struct DumpResponse {
    std::string requestId;       // Matching request identifier
    bool success;               // Operation success status
    std::string outputFile;     // Path to generated dump file
    std::string errorMessage;   // Error description if failed
};
```

### File Transfer Mechanism
The service implements efficient file transfer with:
- Chunked reading/writing (64KB chunks by default)
- Atomic file replacement using temporary files
- Proper error handling and cleanup
- fsync() for data durability

### Error Handling Strategy
1. **Service Unavailable**: Falls back to fork-based approach (if enabled)
2. **Communication Timeout**: Retries with exponential backoff
3. **Service Error**: Logs error and falls back (if enabled)
4. **File Transfer Error**: Cleans up partial files and reports failure

### Thread Safety
- Service handles multiple concurrent requests safely
- Master client uses unique request IDs to avoid conflicts
- Proper mutex protection for shared resources

## Performance Considerations

### Memory Usage
- Service runs independently, not affecting master memory
- Configurable buffer sizes for file operations
- Automatic cleanup of completed requests

### CPU Usage
- Service can be assigned different CPU priorities
- Master server CPU freed during dump operations
- Parallel processing capability for multiple dumps

### I/O Optimization
- Chunked file transfers reduce memory pressure
- Atomic operations minimize filesystem inconsistencies
- Configurable I/O buffer sizes

## Security Considerations

### Access Control
- Unix domain socket provides process-level security
- Service runs with minimal required privileges
- Socket permissions restrict access to authorized processes

### Data Protection
- Temporary files use secure naming conventions
- Atomic file operations prevent partial writes
- Proper cleanup of sensitive data in memory

## Compatibility Matrix

| Configuration | Master Behavior | Service Required | Fallback Available |
|---------------|----------------|------------------|-------------------|
| Default | Fork-based | No | N/A |
| Service + Fallback | Service → Fork | Yes (optional) | Yes |
| Service Only | Service | Yes (required) | No |
| Service Disabled | Fork-based | No | N/A |

## Future Enhancements

### Potential Improvements
1. **Remote Service Support**: TCP socket communication for distributed deployments
2. **Load Balancing**: Multiple service instances with request distribution
3. **Compression**: Optional compression for file transfers
4. **Metrics Integration**: Prometheus metrics for monitoring
5. **Configuration Reload**: Dynamic configuration updates without restart

### Extension Points
- Protocol versioning for backward compatibility
- Plugin architecture for custom dump formats
- Event hooks for integration with external systems


## File Changes Summary

### New Files Created
```
src/metadumper_lib/
├── metadumper_lib.h          # Library interface
├── metadumper_lib.cc         # Core dumping functionality
└── CMakeLists.txt           # Build configuration

src/metadumper_service/
├── metadumper_service.h      # Service implementation header
├── metadumper_service.cc     # Service implementation
├── protocol.h               # Communication protocol definitions
├── protocol.cc              # Protocol implementation
├── file_transfer.h          # File transfer interface
├── file_transfer.cc         # File transfer implementation
├── main.cc                  # Service entry point
└── CMakeLists.txt           # Build configuration

src/master/
├── metadumper_client.h       # Service client interface
├── metadumper_client.cc      # Service client implementation
├── metadumper_config.h       # Configuration management interface
└── metadumper_config.cc      # Configuration management implementation

etc/
└── metadumper.conf          # Example configuration file
```

### Modified Files
```
CMakeLists.txt                        # Added new subdirectories
src/master/metadata_dumper_file.h     # Added service integration
src/master/metadata_dumper_file.cc    # Implemented service communication
```

## Conclusion

This refactoring successfully decouples metadata dumping from the master server process while maintaining full backward compatibility. The implementation provides:

1. **Zero Breaking Changes**: Existing deployments continue working unchanged
2. **Flexible Migration Path**: Gradual adoption with fallback safety
3. **Improved Architecture**: Better separation of concerns and resource management
4. **Operational Benefits**: Independent scaling and resource allocation
5. **Future-Proof Design**: Extensible architecture for future enhancements

The feature flag approach (`METADUMPER_USE_SERVICE_ARCHITECTURE`) ensures that organizations can adopt the new architecture at their own pace, with the ability to rollback instantly if needed.

### Key Success Factors
- **Backward Compatibility**: Original behavior preserved by default
- **Safety First**: Fallback mechanisms prevent service disruption
- **Comprehensive Testing**: Configuration system thoroughly validated
- **Clear Documentation**: Complete deployment and migration guidance
- **Monitoring Support**: Detailed logging for operational visibility

### Recommended Adoption Strategy
1. Deploy in test environments first
2. Enable with fallback in production
3. Monitor service performance and stability
4. Gradually disable fallback once confident
5. Consider advanced features like remote services

This refactoring positions SaunaFS for better scalability and operational flexibility while respecting the need for stability and backward compatibility in production environments.

---

*For questions or issues related to this refactoring, please refer to the SaunaFS documentation or contact the development team.*

