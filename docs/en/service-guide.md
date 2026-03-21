# Service Guide

## 0x10 Session

Used to switch diagnostic sessions. It is usually the prerequisite for other sensitive services.

## 0x11 ECU Reset

Uses a two-stage flow:

- stage 1 validates the request and returns a positive response
- stage 2 performs the physical reset after the response is sent

## 0x22 / 0x2E DID parameter service

This is a project adaptation layer, not a universal parameter database. You should replace it with your own parameter or NVM backend.

## 0x27 Security Access

The current implementation provides a demo-level Seed/Key mechanism. It is suitable for integration testing, but should not be used directly as a production security solution.

## 0x28 Communication Control

Used to disable or restore application RX/TX behavior. It is useful for download, flashing, or silent-test scenarios.

## 0x2A Periodic Data

PDIDs are registered through a provider model, and the 0x2A service then manages subscription and scheduling.

## 0x2F IO Control

Provides DID-to-I/O-handler mapping and supports automatic `ReturnControlToECU` when the diagnostic session times out.

## 0x31 Remote Console

Wraps shell command execution as a Routine and returns the command output.

## 0x34 ~ 0x38 File Transfer

Designed for file read/write workflows. Supports:

- request file transfer
- transfer data
- request transfer exit
- CRC32 verification at the end
