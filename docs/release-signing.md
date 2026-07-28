# Release signing

The production Android App Bundle is signed with the Lumen Blocks upload key.
The private keystore and its password file are intentionally excluded from Git.

## Local files

- Keystore: `release/lumen-blocks-upload.jks`
- Credentials: `keystore.properties`
- Alias: `lumenblocks-upload`

Back up both local files in a secure password manager or encrypted archive.
Future Play Store updates must be signed with the same upload key unless Google
approves an upload-key reset.

## Upload certificate

Owner:

```text
CN=Jdrexx, OU=Lumen Blocks, O=Jdrexx, L=Los Angeles, ST=California, C=US
```

SHA-1:

```text
0B:2C:B0:84:CB:92:4E:4F:8B:16:C5:B6:6E:2F:55:10:2E:A1:FF:31
```

SHA-256:

```text
AC:4B:9A:67:64:F6:45:85:65:1C:EF:E2:FE:62:14:C5:65:EB:5E:A9:5B:11:CE:55:FA:50:73:15:BF:18:88:64
```

The upload certificate is self-signed, as expected for an Android upload key.
Google Play App Signing uses Google's separate app-signing certificate for
distributed APKs.

## Build

```bash
./gradlew bundleRelease
```

Output:

```text
app/build/outputs/bundle/release/app-release.aab
```
