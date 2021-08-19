#ifndef _CONCORDIUM_APP_EXPORT_PRIVATE_KEY_SEED_H_
#define _CONCORDIUM_APP_EXPORT_PRIVATE_KEY_SEED_H_

/**
 * Handles the export of private keys (that are used as key seeds) that are allowed to leave the device.
 * The export paths are restricted so that the method cannot access any account paths.
 * @param p1 has to be 0x00 for export of idcredsec, 0x01 for export of prf-key or 0x02 for export 
 * of anonymity revoker decryption key
 */ 
void handleExportPrivateKeySeed(uint8_t *dataBuffer, uint8_t p1, volatile unsigned int *flags);

typedef struct {
    uint8_t display[15];
    uint32_t path[6];
    uint8_t pathLength;
} exportPrivateKeySeedContext_t;

#endif
