# Boot Failure Detection - Manual Verification

## Preconditions
- System services installed via `scripts/recovery/install-system-recovery-services.sh`.
- Recovery entry configured in bootloader.
- Root filesystem uses Btrfs with `/.snapshots`.

## Scenario 1: Simulate failed boot threshold
1. Set counter near threshold:
   - `sudo sh -c 'echo 2 > /boot/bootcount'`
2. Trigger increment + check manually:
   - `sudo /usr/local/lib/slm-recovery/bootcount-guard.sh increment`
   - `sudo /usr/local/lib/slm-recovery/trigger-recovery-on-boot-failure.sh`
3. Expected:
   - `/boot/bootcount` is `3`
   - `/var/log/boot-recovery.log` contains `threshold exceeded`
   - next boot entry set to recovery (`bootctl set-oneshot` or `grub-reboot` path)

## Scenario 2: Simulate kernel panic recovery path
1. Simulate persistent failures by setting threshold directly:
   - `sudo sh -c 'echo 3 > /boot/bootcount'`
2. Run check:
   - `sudo /usr/local/lib/slm-recovery/trigger-recovery-on-boot-failure.sh`
3. Expected:
   - recovery trigger script requests recovery entry
   - system reboots to recovery flow

## Scenario 3: Simulate success reset
1. Mark successful boot:
   - `sudo /usr/local/lib/slm-recovery/bootcount-guard.sh success`
2. Expected:
   - `/boot/bootcount` becomes `0`
   - `/boot/last_good_snapshot` updated with active subvolume name
   - `/var/log/boot-recovery.log` contains `boot success marked`

## Scenario 4: Snapshot restore command
1. Create test snapshot:
   - `sudo scripts/recovery/recoveryctl.sh snapshot create manual smoke`
2. Restore:
   - `sudo scripts/recovery/recoveryctl.sh snapshot restore <snapshot-id> --yes`
3. Expected:
   - default subvolume points to selected snapshot
   - reboot required

## Scenario 5: Bootloader repair command
1. Run:
   - `sudo scripts/recovery/recoveryctl.sh boot repair`
2. Expected:
   - `grub-install` success
   - grub config regenerated
   - recovery and boot logs updated

## Scenario 6: Manual override disables auto recovery
1. Create override marker:
   - `sudo touch /boot/recovery.auto.disable`
2. Set threshold reached:
   - `sudo sh -c 'echo 3 > /boot/bootcount'`
3. Run trigger:
   - `sudo /usr/local/lib/slm-recovery/trigger-recovery-on-boot-failure.sh`
4. Expected:
   - no forced reboot to recovery
   - log contains `auto recovery disabled by override`
5. Cleanup:
   - `sudo rm -f /boot/recovery.auto.disable`

## Scenario 7: Anti-loop lock after max recovery attempts
1. Simulate repeated attempts:
   - `sudo sh -c 'echo 3 > /boot/recovery_attempts'`
   - `sudo sh -c 'echo 3 > /boot/bootcount'`
2. Run trigger:
   - `sudo /usr/local/lib/slm-recovery/trigger-recovery-on-boot-failure.sh`
3. Expected:
   - `/boot/recovery_auto.lock` created
   - no infinite recovery reboot loop
4. Clear lock:
   - `sudo scripts/recovery/recoveryctl.sh boot clear-state`
