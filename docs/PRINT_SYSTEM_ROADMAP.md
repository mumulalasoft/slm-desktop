# Roadmap Implementasi System Print UI (SLM)

## 1. Ringkasan Visi Produk
- Membangun **system print dialog** yang konsisten untuk semua aplikasi, dengan UX modern ala macOS namun native Linux.
- Menjadikan **PDF-first** sebagai jalur utama agar preview dan output final identik.
- Menggunakan UI **capability-driven** dari CUPS/IPP, bukan hardcode model printer.
- Menyediakan area **Printer Features** berbasis plugin deklaratif yang aman.
- Menyiapkan fondasi agar mudah diintegrasikan ke **portal print backend** di fase lanjut.

## 2. Prinsip Desain
- Secure by default, least privilege, no arbitrary vendor native widget.
- Default view sederhana: printer, copies, page range, color, duplex, paper size, print.
- Detail view kuat tapi tetap terstruktur: layout, paper handling, quality, color, job options, plugin features.
- Preview harus selalu berasal dari jalur render yang sama dengan print final.
- UI dan backend dipisah tegas: QML presentasi, C++ orchestration dan printing stack.
- Capability discovery dari CUPS/IPP modern (IPP Everywhere) sebagai baseline utama.

## 3. Arsitektur Tingkat Tinggi
- `PrintSystemService` (C++): orchestration session print.
- `PrinterManager`: daftar printer, default printer, status.
- `PrinterCapabilityProvider`: query IPP/CUPS, normalisasi capability.
- `PrintSession`: state runtime per dialog invocation.
- `PrintSettingsModel`: state terpadu UI + tiket print.
- `PrintPreviewModel`: render/caching halaman preview.
- `PrintJobBuilder`: mapping settings -> job ticket/IPP attrs.
- `PluginFeatureResolver`: load+validate plugin descriptor, expose feature model.
- `PrintTicketSerializer`: persist/restore preset, per-printer defaults.
- `JobSubmitter`: submit CUPS job, fallback pipeline.
- `QueueObserver` (post-MVP): monitor progress/error lanjutan.
- UI QML:
  - `PrintDialogWindow.qml`
  - `PrintDialogBasicPane.qml`
  - `PrintDialogDetailPane.qml`
  - `PrintPreviewPane.qml`
  - `PrinterFeatureSection.qml`

## 4. Roadmap Fase demi Fase

### Phase 0: Discovery & Contracts
**Tujuan**
- Menetapkan kontrak data, API, dan batas keamanan.
**Deliverables**
- RFC data model (`PrinterCapability`, `PrintSettingsModel`, `PrintTicket`).
- Mapping table capability -> control UI.
- Dokumen plugin schema draft v1.
**Exit criteria**
- Semua interface inti disetujui tim.

### Phase 1: Core Backend Foundation (MVP-1)
**Tujuan**
- Pipeline backend siap tanpa UI kompleks.
**Deliverables**
- `PrinterManager`, `PrinterCapabilityProvider`, `PrintSettingsModel`.
- `PrintJobBuilder` + `JobSubmitter` basic.
- CUPS submit PDF sederhana.
**Exit criteria**
- Bisa submit job ke printer default via API internal.

### Phase 2: Basic Print Dialog (MVP-2)
**Tujuan**
- Dialog sistem sederhana usable end-to-end.
**Deliverables**
- UI basic mode + state sync.
- Binding ke capability yang tersedia.
- Error/loading state dasar.
**Exit criteria**
- User bisa print dokumen dengan opsi dasar konsisten.

### Phase 3: Preview Pipeline (MVP-3)
**Tujuan**
- Preview akurat dan sinkron.
**Deliverables**
- `PrintPreviewModel` page navigation + zoom + fit.
- Preview cache per page + incremental render.
**Exit criteria**
- Perubahan setting langsung tercermin di preview dan hasil print.

### Phase 4: Detail Mode + Capability Expansion
**Tujuan**
- Mode advanced yang lengkap.
**Deliverables**
- Panel detail (layout, handling, quality, color, job options).
- Fallback ketika capability tidak tersedia.
**Exit criteria**
- Advanced users dapat kontrol penting tanpa merusak UX default.

### Phase 5: Plugin Feature System (Post-MVP awal)
**Tujuan**
- Menambahkan printer/vendor feature secara aman.
**Deliverables**
- Descriptor parser + validator + resolver.
- UI section `Printer Features`.
- Mapping feature -> IPP job attributes.
**Exit criteria**
- Feature tambahan muncul declarative tanpa native code injection.

### Phase 6: Persistence, Preset, Hardening
**Tujuan**
- Menstabilkan pengalaman harian.
**Deliverables**
- Last-used per printer, preset management, serializer.
- Accessibility, keyboard nav, responsive polish.
- Better error mapping untuk offline/unavailable printer.
**Exit criteria**
- Dialog production-ready untuk pemakaian desktop harian.

### Phase 7: Portal Print Integration Readiness
**Tujuan**
- Menyiapkan integrasi ke xdg-desktop-portal path.
**Deliverables**
- Adapter boundary API request/session style.
- Non-blocking async flow + cancellation.
**Exit criteria**
- Print dialog bisa dipanggil sebagai system backend lintas aplikasi.

## 5. Milestone Implementasi
1. M1: Model & API contracts selesai.
2. M2: Query printer + capability via CUPS/IPP.
3. M3: Submit PDF basic ke CUPS.
4. M4: Basic dialog mode berfungsi.
5. M5: Preview sync (page nav + zoom + fit).
6. M6: Detail mode aktif capability-driven.
7. M7: Plugin descriptor v1 + validasi.
8. M8: Preset/default per-printer.
9. M9: UX/accessibility hardening.
10. M10: Portal adapter integration ready.

## 6. Modul Teknis yang Harus Dibuat
- `src/printing/core/PrinterManager.{h,cpp}`
- `src/printing/core/PrinterCapabilityProvider.{h,cpp}`
- `src/printing/core/PrintSession.{h,cpp}`
- `src/printing/core/PrintSettingsModel.{h,cpp}`
- `src/printing/core/PrintPreviewModel.{h,cpp}`
- `src/printing/core/PrintJobBuilder.{h,cpp}`
- `src/printing/core/PrintTicketSerializer.{h,cpp}`
- `src/printing/core/JobSubmitter.{h,cpp}`
- `src/printing/core/PluginFeatureResolver.{h,cpp}`
- `src/printing/core/QueueObserver.{h,cpp}` (optional lanjut)
- `Qml/apps/printdialog/*` untuk UI layer.

## 7. Kontrak API yang Direkomendasikan

```cpp
class PrinterManager : public QObject {
  Q_OBJECT
public:
  Q_INVOKABLE QList<PrinterInfo> listPrinters() const;
  Q_INVOKABLE PrinterInfo defaultPrinter() const;
  Q_INVOKABLE PrinterStatus status(const QString& printerId) const;
};

class PrinterCapabilityProvider : public QObject {
  Q_OBJECT
public:
  Q_INVOKABLE PrinterCapability capability(const QString& printerId) const;
};

class PrintSession : public QObject {
  Q_OBJECT
public:
  Q_INVOKABLE void start(const QString& documentUriOrPdfPath);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE PrintSettingsModel* settings();
  Q_INVOKABLE PrintPreviewModel* preview();
  Q_INVOKABLE PrintSubmitResult submit();
};
```

## 8. Struktur Model Data

```cpp
struct PrinterCapability {
  QString printerId;
  bool supportsPdfDirect;
  QStringList paperSizes;
  QStringList trays;
  QStringList colorModes;
  QStringList duplexModes;
  QStringList qualityModes;
  QList<int> resolutionsDpi;
  QVariantMap finishingOptions;
  QVariantMap vendorExtensions;
};

struct PrintSettings {
  QString printerId;
  int copies;
  QString pageRange;
  QString paperSize;
  QString orientation;
  QString duplex;
  QString colorMode;
  QString quality;
  double scale;
  QVariantMap pluginFeatures;
};
```

## 9. Desain Sistem Plugin Printer
- Format descriptor: JSON schema internal v1.
- Sumber:
  1. IPP standard options
  2. Vendor extension mapped (allowlist)
  3. External descriptor signed/trusted source (post-MVP)
- Field types:
  - `bool`, `enum`, `segmented`, `range`, `group`
- Aturan keamanan:
  - Tidak boleh load native code arbitrary.
  - Tidak boleh akses filesystem/network dari descriptor.
  - Hanya declarative field + validation rules.
- Fallback:
  - Descriptor invalid -> section disembunyikan + log warning.

Contoh descriptor ringkas:
```json
{
  "id": "vendor.features",
  "title": "Printer Features",
  "fields": [
    {"id":"toner_save","type":"bool","label":"Toner Save"},
    {"id":"staple","type":"enum","label":"Staple","values":["none","left","dual"]}
  ],
  "map_to_ipp": {
    "toner_save": "vendor-toner-save",
    "staple": "finishings"
  }
}
```

## 10. Strategi Integrasi CUPS/IPP
- Discovery printer via CUPS APIs + IPP attrs.
- Avahi dipakai untuk network discovery tambahan jika diperlukan.
- Capability query:
  - media-supported, sides-supported, print-color-mode-supported, print-quality-supported, printer-resolution-supported, dst.
- Submit:
  - prioritas PDF direct.
  - fallback filter/raster hanya saat capability PDF direct tidak ada.
- Error mapping:
  - offline, paused, denied, unsupported option -> pesan user-friendly.

## 11. Strategi Preview / PDF Pipeline
- Satu sumber render canonical: PDF intermediate.
- Preview membaca PDF yang sama dengan yang akan disubmit.
- Perubahan setting yang mempengaruhi layout menghasilkan regenerasi preview yang terkontrol.
- Cache per halaman + invalidation selective.
- Incremental rendering untuk dokumen panjang.
- Loading/error skeleton states wajib untuk UX smooth.

## 12. Risiko Teknis dan Mitigasi
- **Risk:** capability antar printer tidak konsisten.
  - Mitigasi: normalisasi model + fallback UI rules.
- **Risk:** preview mismatch vs output.
  - Mitigasi: jalur PDF-first tunggal.
- **Risk:** plugin vendor unsafe.
  - Mitigasi: declarative-only + schema validation + allowlist.
- **Risk:** performa preview dokumen besar.
  - Mitigasi: incremental render + cache + async workers.
- **Risk:** legacy printer behavior aneh.
  - Mitigasi: fallback minimal + not-supported hint, fokus modern printer untuk MVP.

## 13. Prioritas MVP vs Post-MVP

### Wajib MVP
- Basic dialog mode.
- CUPS printer discovery + default + status.
- PDF submit basic.
- Preview core (page nav + zoom/fit).
- Capability-driven controls dasar (paper/duplex/color/copies/range).

### Post-MVP
- Plugin feature system lengkap.
- Queue observer detail.
- Preset sharing/import-export.
- Vendor extension advanced mapping.
- Portal backend integration penuh.

## 14. Checklist Validasi UX
- Default mode tidak menakutkan (opsi inti saja).
- Show Details jelas dan reversible.
- Active printer jelas terlihat.
- Preview cepat muncul, tidak blank lama.
- Perubahan setting terasa instant dan sinkron.
- Keyboard-only flow usable.
- A11y labels/roles lengkap.
- Multi-monitor window behavior benar.
- Error messages non-teknis dan actionable.

## 15. Checklist Validasi Teknis
- Unit tests:
  - capability normalizer
  - ticket serializer
  - plugin descriptor validator
- Integration tests:
  - CUPS discovery
  - submit PDF
  - unsupported option handling
- UI tests:
  - basic/detail toggle
  - preview state transitions
- Regression matrix:
  - PDF-direct printer
  - printer fallback non-PDF
  - offline/paused printer
- Security checks:
  - descriptor sanitization
  - no arbitrary code execution path

---

## Prioritas Implementasi Paling Aman (Rekomendasi)
1. Core model + CUPS capability provider
2. Basic dialog + basic submit
3. Preview pipeline PDF-first
4. Detail mode capability-driven
5. Persisted settings/preset
6. Plugin declarative system
7. Portal integration adapter

Trade-off utama:
- Menunda legacy richness lebih aman daripada membuka scope luas di awal.
- PDF-first + capability normalization memberi fondasi paling stabil untuk kualitas UX jangka panjang.

---

## Sprint Backlog (Siap Eksekusi)

### Sprint 1 (Foundation Contracts)
**Goal**
- Kontrak data + service skeleton siap dipakai UI.
**Tasks**
- Buat `PrinterCapability`, `PrintSettings`, `PrintTicket` model C++.
- Buat `PrintSettingsModel` + validasi constraints.
- Buat `PrintSession` skeleton lifecycle (start/cancel/submit state).
- Tambah unit test untuk serializer/deserializer settings.
**Dependency**
- Tidak bergantung CUPS full; pakai mock provider.
**Exit Criteria**
- API internal stabil, test contract pass.

### Sprint 2 (CUPS/IPP Capability + Printer List)
**Goal**
- Printer discovery dan capability read berjalan.
**Tasks**
- Implement `PrinterManager` (list/default/status).
- Implement `PrinterCapabilityProvider` (IPP attrs -> normalized model).
- Mapping fallback saat capability kosong/tidak lengkap.
- Tambah integration test terhadap CUPS test instance.
**Dependency**
- Sprint 1 model siap.
**Exit Criteria**
- UI demo bisa pilih printer dan menampilkan opsi capability dasar.

### Sprint 3 (Basic Dialog MVP)
**Goal**
- End-to-end print dialog sederhana usable.
**Tasks**
- Implement QML:
  - `PrintDialogWindow.qml`
  - `PrintDialogBasicPane.qml`
  - `PrinterSelector.qml`
- Wiring instant-apply ke `PrintSettingsModel`.
- Submit PDF job sederhana via `JobSubmitter`.
**Dependency**
- Sprint 2 backend capability + printer list.
**Exit Criteria**
- User bisa print via basic mode ke printer default/non-default.

### Sprint 4 (Preview Core PDF-First)
**Goal**
- Preview sinkron dengan print output.
**Tasks**
- Implement `PrintPreviewModel` (page nav, zoom, fit page/width).
- Implement cache halaman + invalidation selective.
- Integrasi loading/error states di `PrintPreviewPane.qml`.
**Dependency**
- Sprint 3 dialog + submit flow.
**Exit Criteria**
- Perubahan setting penting tercermin di preview dan hasil print final.

### Sprint 5 (Detail Mode Capability-Driven)
**Goal**
- Advanced settings kuat tanpa mengganggu basic mode.
**Tasks**
- Implement detail panels:
  - Layout
  - Paper Handling
  - Quality
  - Color
  - Job Options
- Visibility panel berdasarkan capability normalized.
- Keyboard navigation + accessibility review awal.
**Dependency**
- Sprint 2 capability map, Sprint 4 preview.
**Exit Criteria**
- Advanced mode aktif, tetap stabil untuk berbagai printer.

### Sprint 6 (Persistence + Preset)
**Goal**
- UX harian lebih cepat dan konsisten.
**Tasks**
- Implement `PrintTicketSerializer`.
- Last-used settings per printer.
- Preset save/load.
- Conflict resolution saat printer capability berubah.
**Dependency**
- Sprint 5 controls final.
**Exit Criteria**
- User preferences tersimpan dan direstore valid.

### Sprint 7 (Plugin Feature Declarative v1)
**Goal**
- Printer Features extensible dan aman.
**Tasks**
- Implement schema JSON plugin + validator.
- Implement `PluginFeatureResolver`.
- Integrasi `PrinterFeatureSection.qml`.
- Mapping ke IPP job attributes.
**Dependency**
- Sprint 2 capability layer + Sprint 5 detail pane.
**Exit Criteria**
- Satu set fitur vendor deklaratif tampil dan ikut ke print ticket.

### Sprint 8 (Hardening + Observability)
**Goal**
- Production-ready quality.
**Tasks**
- Tambah `QueueObserver` basic.
- Error mapping detail ke UI (offline/paused/unsupported).
- Telemetry ringan + tracing debug mode.
- Regression matrix test modern printers + fallback.
**Dependency**
- Sprint 3–7.
**Exit Criteria**
- Stability target terpenuhi, known issues terdokumentasi.

### Sprint 9 (Portal Adapter Readiness)
**Goal**
- Siap dipakai sebagai backend print dialog lintas aplikasi.
**Tasks**
- Define portal-facing adapter boundary (request/session).
- Async cancellation-safe flow.
- Integrasi policy/permission path sesuai foundation.
**Dependency**
- Sprint 8 stable.
**Exit Criteria**
- Contract siap integrasi ke backend portal print.

---

## Parallel Workstream Rekomendasi

1. **Core Backend Track**
- Sprint 1–2 fokus model/capability/CUPS.

2. **UI/UX Track**
- Sprint 3–5 fokus dialog, preview, accessibility.

3. **Extensibility & Security Track**
- Sprint 6–9 fokus persistence, plugin security, portal readiness.

---

## Ready-to-Start Task List (Top 12)

1. Buat `src/printing/core/PrintSettingsModel.{h,cpp}`.
2. Buat `src/printing/core/PrinterCapabilityTypes.h`.
3. Buat `src/printing/core/PrintSession.{h,cpp}`.
4. Buat mock `PrinterManager` + unit tests.
5. Buat CUPS adapter minimal (`listPrinters/defaultPrinter`).
6. Buat `Qml/apps/printdialog/PrintDialogWindow.qml` shell layout.
7. Buat `Qml/apps/printdialog/PrintDialogBasicPane.qml`.
8. Wiring basic controls -> `PrintSettingsModel`.
9. Buat `JobSubmitter` minimal PDF submit.
10. Buat `PrintPreviewModel` interface (stub render).
11. Tambah contract tests serializer settings.
12. Tambah smoke test basic print dialog open/close + printer selection.
