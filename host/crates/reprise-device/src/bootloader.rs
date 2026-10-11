// SPDX-License-Identifier: GPL-3.0-only

use crate::{invalid, Result, APPLE_LOADER_BYTES};
use serde::{Deserialize, Serialize};
use sha1::Sha1;
use sha2::{Digest, Sha256};

#[derive(Clone, Debug, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct BootloaderFingerprint {
    pub bytes: usize,
    pub sha256: String,
    pub entrypoint: u32,
}

impl BootloaderFingerprint {
    pub fn from_code(code: &[u8], entrypoint: u32) -> Self {
        Self {
            bytes: code.len(),
            sha256: format!("{:x}", Sha256::digest(code)),
            entrypoint,
        }
    }

    pub fn validate(&self) -> Result<()> {
        if self.bytes < 64
            || self.bytes > APPLE_LOADER_BYTES
            || self.bytes % 16 != 0
            || (self.entrypoint & !1) as usize >= self.bytes
            || self.sha256.len() != 64
            || !self
                .sha256
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        {
            return Err(invalid("Invalid bootloader fingerprint"));
        }
        Ok(())
    }
}

#[derive(Debug, Serialize)]
pub struct BootloaderReport {
    pub fingerprint: BootloaderFingerprint,
    pub apple: bool,
    pub read_bytes: usize,
}

pub(crate) fn progress(completed: usize) -> crate::NorProgress {
    crate::NorProgress {
        stage: "bootloader",
        completed,
        total: 100,
    }
}

pub(crate) fn inspection_progress(
    p: crate::NorProgress,
    start: usize,
    end: usize,
) -> crate::NorProgress {
    let fraction = p.completed.min(p.total) * 100 / p.total.max(1);
    let percent = match p.stage {
        "nor_read" => fraction / 2,
        "nor_verify" => 50 + fraction * 49 / 100,
        "decrypt" => fraction * 99 / 100,
        _ => 0,
    };
    progress(start + (end - start) * percent / 100)
}

pub(crate) fn header_size(header: &[u8]) -> Result<usize> {
    if header.len() != 0x800 || &header[..7] != b"87021.0" || !matches!(header[7], 1 | 2) {
        return Err(invalid(
            "Unrecognised NOR bootloader header; automatic update refused",
        ));
    }
    let size = u32::from_le_bytes(header[12..16].try_into().unwrap()) as usize;
    let entrypoint = u32::from_le_bytes(header[8..12].try_into().unwrap());
    if !(64..=APPLE_LOADER_BYTES).contains(&size)
        || size % 16 != 0
        || (entrypoint & !1) as usize >= size
    {
        return Err(invalid("Invalid NOR bootloader size or entrypoint"));
    }
    Ok(size)
}

pub(crate) fn verify(
    header: &[u8],
    body: &[u8],
    mut decrypt: impl FnMut(&[u8]) -> Result<Vec<u8>>,
) -> Result<BootloaderReport> {
    let size = header_size(header)?;
    if body.len() != size {
        return Err(invalid("Truncated NOR bootloader"));
    }
    if decrypt(&header[64..80])? != Sha1::digest(&header[..64])[..16] {
        return Err(invalid("NOR bootloader header signature mismatch"));
    }
    let plain = if header[7] == 1 {
        decrypt(body)?
    } else {
        body.to_vec()
    };
    let hash = Sha1::digest(&plain);
    if plain.len() != size || decrypt(&header[16..32])? != hash[..16] {
        return Err(invalid("NOR bootloader code signature mismatch"));
    }
    // The same Classic Apple loader identities accepted by the dual-boot installer.
    let short_hash: String = hash[..16].iter().map(|b| format!("{b:02x}")).collect();
    let apple = [
        "31d17a3a4b05e4096b58310ab76c2f88",
        "7cef11d13a96ba1c1c734291f3fcbf20",
        "91e7210dfa4d19f8061eac96c70d4113",
        "666676dc1d32b246a6c97d5a61d3494c",
        "14c54c28486604252fb6d717952c97ce",
        "1ef0d9dec27eec027c1576bb5c4f2d95",
        "9c84ada46b38c325b12e32a1d1336d2d",
        "0685df28e4d7f482c073b05326fcb0fe",
        "60807d33a8def849bbbe0145ff624019",
    ]
    .contains(&short_hash.as_str());
    if apple {
        crate::nor_image::validate_volume(&plain)?;
    }
    Ok(BootloaderReport {
        fingerprint: BootloaderFingerprint::from_code(
            &plain,
            u32::from_le_bytes(header[8..12].try_into().unwrap()),
        ),
        apple,
        read_bytes: header.len() + body.len(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn inspection_progress_never_restarts_or_finishes_before_verification() {
        for decryptions in [2, 3] {
            let mut events = vec![progress(0)];
            for (start, end) in [(0, 5), (5, 90)] {
                for stage in ["echo", "nor_read", "nor_verify"] {
                    for completed in [0, 1, 25, 50, 99, 100] {
                        events.push(inspection_progress(
                            crate::NorProgress {
                                stage,
                                completed,
                                total: 100,
                            },
                            start,
                            end,
                        ));
                    }
                }
            }
            for index in 0..decryptions {
                for stage in ["echo", "decrypt"] {
                    for completed in [0, 1, 8, 16] {
                        events.push(inspection_progress(
                            crate::NorProgress {
                                stage,
                                completed,
                                total: 16,
                            },
                            90 + 9 * index / decryptions,
                            90 + 9 * (index + 1) / decryptions,
                        ));
                    }
                }
            }
            assert!(events.iter().all(|p| p.completed < 100 && p.total == 100));
            events.push(progress(100));
            assert!(events.windows(2).all(|p| p[0].completed <= p[1].completed));
        }
    }

    fn image() -> (Vec<u8>, Vec<u8>) {
        let body = vec![0x42; 128];
        let mut header = vec![0; 0x800];
        header[..8].copy_from_slice(b"87021.0\x02");
        header[12..16].copy_from_slice(&(body.len() as u32).to_le_bytes());
        header[16..32].copy_from_slice(&Sha1::digest(&body)[..16]);
        let hash = Sha1::digest(&header[..64]);
        header[64..80].copy_from_slice(&hash[..16]);
        (header, body)
    }

    #[test]
    fn signed_code_and_entrypoint_identify_custom_loader() {
        let (header, body) = image();
        let report = verify(&header, &body, |b| Ok(b.to_vec())).unwrap();
        assert!(!report.apple);
        assert_eq!(report.read_bytes, 0x800 + body.len());
        assert_eq!(
            report.fingerprint,
            BootloaderFingerprint::from_code(&body, 0)
        );
        for index in [8, 16, 64] {
            let mut corrupt = header.clone();
            corrupt[index] ^= 1;
            assert!(verify(&corrupt, &body, |b| Ok(b.to_vec())).is_err());
        }
        let mut corrupt = body.clone();
        corrupt[0] ^= 1;
        assert!(verify(&header, &corrupt, |b| Ok(b.to_vec())).is_err());
        assert!(verify(&header, &body[..64], |b| Ok(b.to_vec())).is_err());
        assert!(verify(&header, &body, |_| Err(invalid("cancelled"))).is_err());
    }

    #[test]
    fn corrupt_lengths_are_rejected_before_body_reads() {
        let (header, _) = image();
        for length in [0, 15, 63, APPLE_LOADER_BYTES as u32 + 16, u32::MAX] {
            let mut bad = header.clone();
            bad[12..16].copy_from_slice(&length.to_le_bytes());
            assert!(header_size(&bad).is_err());
        }
        let mut bad = header.clone();
        bad[8..12].copy_from_slice(&128u32.to_le_bytes());
        assert!(header_size(&bad).is_err());
        assert!(header_size(&header[..0x100]).is_err());
    }
}
