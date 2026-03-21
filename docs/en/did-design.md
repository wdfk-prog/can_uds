# DID Design Notes

## 1. Goal

The goal of DID design is not to expose every internal variable. It is to build an identifier system that is:

- stable
- evolvable
- able to express access control
- able to serve both diagnostics and production-line workflows

## 2. Recommended layers

It is recommended to divide DIDs into the following groups:

| Type | Purpose | Recommendation |
|---|---|---|
| Fixed identity | SN, version, hardware model | Read-only and as stable as possible across releases |
| Runtime status | Temperature, voltage, status words | Read-only, suitable for `0x22` / `0x2A` |
| Configuration parameters | Thresholds, enable flags, calibration values | Separate read and write permissions, ideally combined with `0x27` |
| Debug / factory | Temporary debug items, internal switches | Use a separate DID range and allow removal in production |
| Virtual control | Used together with `0x2F` | Do not mix with normal parameter DIDs |

## 3. Numbering suggestion

You can reserve ranges such as:

```text
0x0100 ~ 0x01FF   Board-level I/O / demo DIDs
0x1000 ~ 0x1FFF   Product identity information
0x2000 ~ 0x2FFF   Realtime status values
0x3000 ~ 0x3FFF   Writable configuration parameters
0x4000 ~ 0x4FFF   Factory / debug parameters
0xF000 ~ 0xF0FF   Reserved for Routine / Console related usage
```

## 4. Mapping recommendation for this repository

### 4.1 0x22 / 0x2E

`service_0x22_0x2E_param.c` is better suited for configuration DIDs and status DIDs.

### 4.2 0x2F

`service_0x2F_io.c` is better suited for control DIDs, such as:

- LED
- relay
- PWM output
- forced analog output

Do not mix normal parameter writing and realtime control on the same DID.

## 5. Data-format recommendation

- define endianness explicitly
- define engineering units and scaling explicitly
- define read-only / writable properties explicitly
- define whether extended session or security unlock is required
- keep an independent table or document for each DID

## 6. Minimum recommended document fields

| Field | Meaning |
|---|---|
| DID | 16-bit identifier |
| Name | Business-facing name |
| Direction | R / W / RW / IO |
| Length | Fixed or variable |
| Encoding | Integer / ASCII / bitfield / struct |
| Unit | V, mA, degC, etc. |
| Session requirement | default / extended / programming |
| Security requirement | whether unlock is required |
| Notes | compatibility or history notes |
