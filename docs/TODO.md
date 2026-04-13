# SLM Desktop TODO

> Canonical session summary: `docs/SESSION_STATE.md`

## Fokus Aktif (Prioritas Tinggi)

### Storage & Automount
- [ ] QA cold reboot penuh (bukan hanya restart service) untuk persistence policy mount/automount.
- [ ] QA manual lintas kondisi media: locked, busy, unsupported FS, burst multi-partition.

### Startup Performance Desktop
- [ ] Turunkan waktu `qml.load.begin -> main.onCompleted` dengan profiling terarah per komponen.

---

## Program Network & Firewall

### Phase 1 (Wajib)
- [x] NftablesAdapter: jalankan `nft -f -` secara nyata.
- [x] PolicyEngine: reconcile fullstate setelah remove/clear IP policy.
- [x] UI Settings: Security -> Firewall smoke test contract.
  - verified via:
    - `firewallservice_dbus_contract_test`
    - `firewall_policyengine_contract_test`
    - `firewall_nftables_adapter_test`
    - `firewallserviceclient_contract_test`
    - `security_settings_routing_js_test`

### Phase 2
- [ ] Outgoing control (allow/block/prompt) berbasis app identity.
- [ ] Deteksi proses CLI/interpreter (python/node script target, parent/cwd/tty/cgroup).
- [ ] Popup policy yang kontekstual dan tidak spam.

### Phase 3
- [ ] Trust system (System/Trusted/Unknown/Suspicious) + policy adaptif.
- [ ] Behavior learning + auto suggestion.
- [ ] Quarantine mode untuk app mencurigakan.

---

## Notification
- [ ] Hardening multi-monitor behavior.

## Parking Lot (Tidak Aktif Sekarang)
- Struktur Dock: **dibekukan** pada state "last good" sampai siklus pengujian berikutnya.
- Rencana refactor arsitektur shell yang besar: lanjut setelah milestone stabilitas/firewall tercapai.
