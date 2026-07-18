# FormatStickPro

**FormatStickPro** is a professional Android application designed to format USB storage devices directly from your smartphone or tablet. It utilizes native SCSI drivers and industry-standard filesystem tools to provide a robust formatting experience similar to a desktop environment.

[🇷🇴 Citeste versiunea in Romana](#romana) | [🇺🇸 Read the English version](#english)

---

<a name="english"></a>
## 🇺🇸 English Version

### 🌟 Features
*   **Direct USB Control:** Uses `libusb` and a custom SCSI translator to communicate directly with USB Mass Storage devices.
*   **Wide Filesystem Support:**
    *   **NTFS** (via NTFS-3G)
    *   **exFAT** (via exfat-utils)
    *   **Ext2/Ext3/Ext4** (via e2fsprogs)
    *   **F2FS** (via f2fs-tools)
    *   **XFS** (via xfsprogs)
    *   **UDF** (via udftools)
    *   **FAT32/FAT16/FAT12** (Native implementation)
*   **Partitioning:** Supports MBR (Standard) and GPT (Modern) partition tables, or "Super Floppy" mode (no partition table).
*   **Customization:** Choose cluster sizes and volume labels.
*   **Safety:** Integrated safeguards to ensure stable writes and proper buffer flushing.

### 🛠️ Technical Architecture
The app is built with a heavy emphasis on native code (C++) to bridge established Linux filesystem utilities to the Android environment:
*   **Java/Kotlin Layer:** Manages UI, USB permissions, and foreground services.
*   **JNI Bridge:** Interfaces between the Android framework and the native SCSI driver.
*   **Native SCSI Driver:** A custom implementation using `libusb` to bypass the Android OS's limited native filesystem support.
*   **Filesystem Engines:** Compiled static libraries of open-source tools (NTFS-3G, e2fsprogs, etc.).

### 🏗️ Build Instructions
1.  Open the project in **Android Studio**.
2.  Ensure you have the **Android NDK** and **CMake** installed.
3.  The project uses Gradle for builds. Simply "Sync Project with Gradle Files" and run the `app` module.
4.  Native libraries are automatically compiled via the `CMakeLists.txt` in `app/src/main/cpp`.

### ⚖️ License
This project is licensed under the **GNU GPL v3**. See the [LICENSE](LICENSE) file for details.

---

<a name="romana"></a>
## 🇷🇴 Versiunea în Română

### 🌟 Caracteristici
*   **Control Direct USB:** Folosește `libusb` și un translator SCSI custom pentru a comunica direct cu dispozitivele USB Mass Storage.
*   **Suport Larg pentru Sisteme de Fișiere:**
    *   **NTFS** (prin NTFS-3G)
    *   **exFAT** (prin exfat-utils)
    *   **Ext2/Ext3/Ext4** (prin e2fsprogs)
    *   **F2FS** (prin f2fs-tools)
    *   **XFS** (prin xfsprogs)
    *   **UDF** (prin udftools)
    *   **FAT32/FAT16/FAT12** (Implementare nativă)
*   **Partiționare:** Suportă tabele de partiții MBR (Standard) și GPT (Modern), sau modul "Super Floppy" (fără tabelă de partiții).
*   **Personalizare:** Alege dimensiunea clusterului și eticheta volumului (Volume Label).
*   **Siguranță:** Mecanisme integrate pentru a asigura scrieri stabile și golirea corectă a buffer-elor (sync).

### 🛠️ Arhitectură Tehnică
Aplicația pune un accent deosebit pe codul nativ (C++) pentru a aduce utilitarele consacrate de Linux în mediul Android:
*   **Stratul Java/Kotlin:** Gestionează interfața (UI), permisiunile USB și serviciile de fundal (Foreground Services).
*   **Bridge JNI:** Interfața între framework-ul Android și driverul SCSI nativ.
*   **Driver SCSI Nativ:** O implementare proprie folosind `libusb` pentru a ocoli suportul limitat al sistemului Android pentru sisteme de fișiere externe.
*   **Motoare de Formatare:** Biblioteci statice compilate ale uneltelor open-source (NTFS-3G, e2fsprogs, etc.).

### 🏗️ Instrucțiuni de Compilare
1.  Deschide proiectul în **Android Studio**.
2.  Asigură-te că ai instalat **Android NDK** și **CMake**.
3.  Proiectul folosește Gradle. Pur și simplu apasă "Sync Project with Gradle Files" și rulează modulul `app`.
4.  Bibliotecile native sunt compilate automat prin `CMakeLists.txt` aflat în `app/src/main/cpp`.

### ⚖️ Licență
Acest proiect este licențiat sub **GNU GPL v3**. Vezi fișierul [LICENSE](LICENSE) pentru detalii.

---
**Credits:** Developed by FormatStickPro (NotzOs66). Uses various Open Source libraries (NTFS-3G, e2fsprogs, libusb, etc.) as credited in the app's "About" section.
