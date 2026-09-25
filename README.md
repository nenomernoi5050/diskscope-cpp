# DiskScope — read-only content audit for Windows

DiskScope is a C++17 utility for authorized investigations of possible data
leakage. It searches selected directories for policy markers and can inspect a
previously acquired disk image for residual UTF-8 or UTF-16LE signatures.

The project is a safe rewrite of an old filesystem experiment. The public
version contains **no deletion, raw-volume writes, cluster overwriting, or
anti-forensic behavior**.

## What it does

- recursively scans a directory without modifying files;
- searches TXT, CSV, TSV, LOG, JSON, XML and Markdown documents;
- extracts searchable XML from DOCX, XLSX and PPTX containers;
- performs case-insensitive Unicode matching for Latin and Cyrillic text;
- scans an offline image for exact UTF-8 and UTF-16LE byte signatures;
- limits file and ZIP-entry sizes to reduce resource-exhaustion risk;
- uses a bounded worker pool and produces a structured JSON report.

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake downloads the pinned `miniz` 3.0.2 dependency during configuration.

## Usage

Create a UTF-8 keyword file with one policy marker per line:

```text
конфиденциально
ДСП
internal-only
```

Scan an authorized directory:

```powershell
diskscope --root D:\audit-target --keywords keywords.txt --report report.json
```

Inspect an offline image without mounting or modifying it:

```powershell
diskscope --image E:\evidence\workstation.img --keywords keywords.txt --report report.json
```

Image mode performs exact byte matching. Add case variants to the keyword file
when they matter. Results identify offsets and encodings but do not reconstruct,
delete, or overwrite data.

## Safety boundary

DiskScope is a discovery and reporting tool, not a secure-delete utility. It is
designed for authorized DLP checks, incident response and offline forensic
triage. The scanner opens input files in read-only mode and never obtains a raw
write handle.

## Technology

C++17 · CMake · Windows/Linux filesystem APIs · Miniz · OOXML · Unicode ·
multithreading · JSON reporting · automated tests

