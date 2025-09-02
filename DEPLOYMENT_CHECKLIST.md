# SaunaFS Metadumper Service Deployment Checklist

## Pre-Deployment Checklist

### Environment Setup
- [ ] Run `./setup-machine.sh setup /mnt/hdd_01` for proper SaunaFS environment
- [ ] Verify all build dependencies are installed
- [ ] Confirm SaunaFS master is running and stable
- [ ] Backup current configuration files

### Build Verification
- [ ] Successfully built `saunafs-metadumper` service binary
- [ ] Successfully built updated `sfsmaster` with service integration
- [ ] Verified no compilation errors or warnings
- [ ] Tested configuration system with test program

## Deployment Steps

### Phase 1: Preparation (No Service Changes)
- [ ] Deploy new binaries alongside existing ones
- [ ] Verify `METADUMPER_USE_SERVICE_ARCHITECTURE=false` (default)
- [ ] Test metadata dumping with existing fork-based approach
- [ ] Confirm no regression in existing functionality

### Phase 2: Service Installation
- [ ] Copy `saunafs-metadumper` to `/usr/sbin/`
- [ ] Create `/var/run/saunafs/` directory with proper permissions
- [ ] Set ownership: `chown saunafs:saunafs /var/run/saunafs`
- [ ] Copy configuration file to `/etc/saunafs/metadumper.conf`
- [ ] Create systemd service file (if using systemd)

### Phase 3: Service Configuration
- [ ] Edit `/etc/saunafs/metadumper.conf`:
  - [ ] Set `METADUMPER_USE_SERVICE_ARCHITECTURE=true`
  - [ ] Set `METADUMPER_SERVICE_ENABLED=true`
  - [ ] Set `METADUMPER_FALLBACK_ENABLED=true` (safety)
  - [ ] Configure socket path if different from default
- [ ] Validate configuration file syntax
- [ ] Test service startup: `saunafs-metadumper -f` (foreground mode)

### Phase 4: Master Configuration
- [ ] Configure master environment variables:
  - [ ] `METADUMPER_USE_SERVICE_ARCHITECTURE=true`
  - [ ] `METADUMPER_SERVICE_ENABLED=true`
  - [ ] `METADUMPER_FALLBACK_ENABLED=true`
  - [ ] `METADUMPER_SERVICE_SOCKET=/var/run/saunafs/metadumper.sock`
- [ ] Update master service configuration file
- [ ] Verify configuration will be loaded by master process

### Phase 5: Service Startup
- [ ] Start metadumper service: `systemctl start saunafs-metadumper`
- [ ] Verify service is running: `systemctl status saunafs-metadumper`
- [ ] Check service logs for startup messages
- [ ] Verify socket file is created with correct permissions

### Phase 6: Master Integration
- [ ] Restart SaunaFS master: `systemctl restart saunafs-master`
- [ ] Monitor master logs for service architecture messages
- [ ] Verify master can communicate with service
- [ ] Test metadata dump operation

## Post-Deployment Verification

### Functional Testing
- [ ] Trigger metadata dump operation
- [ ] Verify dump completes successfully
- [ ] Check dump file is created in expected location
- [ ] Verify dump file integrity and content
- [ ] Confirm service logs show successful request processing

### Fallback Testing
- [ ] Stop metadumper service temporarily
- [ ] Trigger metadata dump operation
- [ ] Verify fallback to fork-based approach works
- [ ] Check master logs for fallback messages
- [ ] Restart metadumper service

### Performance Monitoring
- [ ] Monitor master server resource usage during dumps
- [ ] Monitor metadumper service resource usage
- [ ] Compare dump times with previous fork-based approach
- [ ] Verify no impact on master server responsiveness

## Monitoring and Maintenance

### Log Monitoring
- [ ] Set up log monitoring for master server
- [ ] Set up log monitoring for metadumper service
- [ ] Configure alerts for service failures
- [ ] Monitor for fallback activation

### Health Checks
- [ ] Verify service socket is accessible
- [ ] Check service process is running
- [ ] Monitor service memory usage
- [ ] Verify dump files are being created regularly

### Backup and Recovery
- [ ] Include service configuration in backup procedures
- [ ] Document rollback procedure
- [ ] Test rollback to fork-based approach
- [ ] Verify service restart procedures

## Rollback Procedure

### Emergency Rollback
If issues occur, immediately rollback by:
- [ ] Set `METADUMPER_USE_SERVICE_ARCHITECTURE=false`
- [ ] Restart SaunaFS master
- [ ] Verify metadata dumping works with fork-based approach
- [ ] Stop metadumper service if desired

### Planned Rollback
For planned rollback:
- [ ] Schedule maintenance window
- [ ] Update configuration to disable service architecture
- [ ] Restart master server
- [ ] Verify functionality
- [ ] Stop and disable metadumper service

## Troubleshooting Guide

### Service Won't Start
- [ ] Check binary permissions and ownership
- [ ] Verify socket directory exists and has correct permissions
- [ ] Check for port/socket conflicts
- [ ] Review service logs for error messages

### Master Can't Connect to Service
- [ ] Verify socket path configuration matches
- [ ] Check socket file permissions
- [ ] Confirm service is running and listening
- [ ] Test socket connectivity manually

### Dumps Failing
- [ ] Check service logs for error details
- [ ] Verify filesystem permissions for dump location
- [ ] Confirm sufficient disk space
- [ ] Test with fallback enabled

### Performance Issues
- [ ] Monitor service resource usage
- [ ] Check for I/O bottlenecks
- [ ] Verify network/socket performance
- [ ] Consider service configuration tuning

## Success Criteria

### Deployment Success
- [ ] Service starts and runs stably
- [ ] Master successfully communicates with service
- [ ] Metadata dumps complete successfully
- [ ] No impact on master server performance
- [ ] Fallback mechanism works when tested

### Production Readiness
- [ ] Service runs for 24+ hours without issues
- [ ] Multiple dump operations complete successfully
- [ ] Monitoring and alerting configured
- [ ] Operations team trained on new architecture
- [ ] Rollback procedure tested and documented

---

**Note**: Always test in a non-production environment first and have a rollback plan ready before deploying to production systems.

