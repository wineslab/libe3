# Information element abstract syntax (with ASN.1)

## General

The abstract syntax of the present document conforms to [2]. It is encoded with the
aligned variant of PER [3] when the E3AP session uses APER (clause 8.5).

If a table of clause 8.2 or 8.3 and the grammar disagree, the grammar takes precedence,
except for the conditions of presence, where the table takes precedence. The comment
next to a field gives its telemetry identifier and its JSON key.

## Information element definitions

```asn1
-- ASN1START
-- E3SM-L1-KPM, version 1

E3SM-L1-KPM-IEs DEFINITIONS AUTOMATIC TAGS ::= BEGIN

-- **************************************************************
-- Constants
-- **************************************************************

maxL1KPMCells       INTEGER ::= 16
maxL1KPMUesPerCell  INTEGER ::= 64
maxL1KPMStreams     INTEGER ::= 128
maxL1KPMFilterUes   INTEGER ::= 64

-- **************************************************************
-- Common types
-- **************************************************************

-- IEEE 754 binary32, least significant octet first.
L1KPM-Float32       ::= OCTET STRING (SIZE (4))

L1KPM-Complex32     ::= SEQUENCE {
    re      L1KPM-Float32,
    im      L1KPM-Float32
}

L1KPM-Uint32        ::= INTEGER (0..4294967295)
L1KPM-Rnti          ::= INTEGER (0..65535)
L1KPM-PhysCellId    ::= INTEGER (0..1007)

-- Bit s (value 2^s) stands for OFDM symbol s of the slot.
L1KPM-SymbolBitmap  ::= INTEGER (0..16383)

L1KPM-TelemetryId   ::= INTEGER (1..65535)

-- Nanoseconds since the epoch of the field. Not constrained, so that a
-- codec can hold it as a signed 64 bit integer.
L1KPM-TimestampNs   ::= INTEGER

-- **************************************************************
-- Shared memory reference
-- **************************************************************

-- JSON keys of the fields, for each stream ("shm_name" in each):
--   ID  stream          bufferIndex              writeIndex              rowByteOffset
--    1  iq_samples      fh_buffer_index          fh_write_index          (absent)
--    2  pdu_data        pusch_buffer_index       pusch_write_index       (absent)
--    3  h_estimates     hest_buffer_index        hest_write_index        hest_row_byte_offset
--   44  srs_iq_samples  srs_iq_buffer_index      srs_iq_write_index      srs_iq_row_byte_offset
--   45  srs_hest        srs_hest_buffer_index    srs_hest_write_index    (absent)
--   49  srs_rb_snr      srs_rb_snr_buffer_index  srs_rb_snr_write_index  (absent)

L1KPM-ShmRef ::= SEQUENCE {
    shmName         OCTET STRING (SIZE (1..63)),
    bufferIndex     INTEGER (0..255),
    writeIndex      L1KPM-Uint32,
    rowByteOffset   L1KPM-Uint32                                OPTIONAL,
    ...
}

-- **************************************************************
-- INDICATION DATA IE: protocolData of the E3 indication
-- **************************************************************

L1KPM-Indication ::= SEQUENCE {
    timestamp       L1KPM-TimestampNs                           OPTIONAL,   --  4 timestamp
    timestampTai    L1KPM-TimestampNs                           OPTIONAL,   -- 78 timestamp_tai
    sfn             INTEGER (0..1023)                           OPTIONAL,   --  5 sfn
    slot            INTEGER (0..159)                            OPTIONAL,   --  6 slot
    nCells          INTEGER (0..maxL1KPMCells)                  OPTIONAL,   -- 10 n_cells
    cells           SEQUENCE (SIZE (1..maxL1KPMCells)) OF L1KPM-CellData
                                                                OPTIONAL,   --    cells
    ...
}

-- **************************************************************
-- CELL DATA IE
-- **************************************************************

L1KPM-CellData ::= SEQUENCE {
    -- shared
    cellId          L1KPM-PhysCellId                            OPTIONAL,   --  7 cell_id
    nRxAntSrs       INTEGER (1..64)                             OPTIONAL,   --  9 n_rx_ant_srs
    -- PUSCH path
    iqSamples       L1KPM-ShmRef                                OPTIONAL,   --  1 iq_samples
    pduData         L1KPM-ShmRef                                OPTIONAL,   --  2 pdu_data
    hEstimates      L1KPM-ShmRef                                OPTIONAL,   --  3 h_estimates
    nRxAnt          INTEGER (1..64)                             OPTIONAL,   --  8 n_rx_ant
    nBsAnts         INTEGER (1..64)                             OPTIONAL,   -- 11 n_bs_ants
    nUe             INTEGER (0..maxL1KPMUesPerCell)             OPTIONAL,   -- 34 n_ue
    -- SRS path
    srsIqSamples    L1KPM-ShmRef                                OPTIONAL,   -- 44 srs_iq_samples
    srsHest         L1KPM-ShmRef                                OPTIONAL,   -- 45 srs_hest
    srsRbSnr        L1KPM-ShmRef                                OPTIONAL,   -- 49 srs_rb_snr
    srsCellStartSym INTEGER (0..13)                             OPTIONAL,   -- 52 srs_cell_start_sym
    srsCellNSrsSym  INTEGER (0..6)                              OPTIONAL,   -- 53 srs_cell_n_srs_sym
    nSrsUe          INTEGER (0..maxL1KPMUesPerCell)             OPTIONAL,   -- 54 n_srs_ue
    -- UE scope
    ues             SEQUENCE (SIZE (1..maxL1KPMUesPerCell)) OF L1KPM-UeData
                                                                OPTIONAL,   --    ues
    -- shared
    validSymbolMask L1KPM-SymbolBitmap                          OPTIONAL,   -- 1000 valid_symbol_mask
    ...
}

-- **************************************************************
-- UE DATA IE
-- **************************************************************

L1KPM-UeData ::= SEQUENCE {
    -- shared
    rnti                L1KPM-Rnti                              OPTIONAL,   -- 33 rnti

    -- PUSCH path: allocation and layers, of the UE group
    rbStart             INTEGER (0..274)                        OPTIONAL,   -- 25 rb_start
    rbSize              INTEGER (1..275)                        OPTIONAL,   -- 26 rb_size
    startSymbolIndex    INTEGER (0..13)                         OPTIONAL,   -- 27 start_symbol_index
    nrOfSymbols         INTEGER (1..14)                         OPTIONAL,   -- 28 nr_of_symbols
    nLayers             INTEGER (1..8)                          OPTIONAL,   -- 12 n_layers
    nSubcarriers        INTEGER (0..3300)                       OPTIONAL,   -- 13 n_subcarriers
    nDmrsEstimates      INTEGER (0..14)                         OPTIONAL,   -- 14 n_dmrs_estimates
    dmrsSymbPos         L1KPM-SymbolBitmap                      OPTIONAL,   -- 15 dmrs_symb_pos
    qamModOrder         INTEGER (1..10)                         OPTIONAL,   -- 22 qam_mod_order
    mcsIndex            INTEGER (0..31)                         OPTIONAL,   -- 23 mcs_index
    mcsTableIndex       INTEGER (0..4)                          OPTIONAL,   -- 24 mcs_table_index

    -- PUSCH path: location of the channel estimates of the UE group
    layerOffset         INTEGER (0..255)                        OPTIONAL,   -- 35 layer_offset
    ueGrpIdx            INTEGER (0..255)                        OPTIONAL,   -- 36 ue_grp_idx
    hOffset             L1KPM-Uint32                            OPTIONAL,   -- 37 h_offset
    hSize               L1KPM-Uint32                            OPTIONAL,   -- 38 h_size

    -- PUSCH path: decoding and HARQ
    tbCrcFail           INTEGER (0..1)                          OPTIONAL,   -- 16 tb_crc_fail
    cbErrors            INTEGER (0..255)                        OPTIONAL,   -- 17 cb_errors
    cbCount             INTEGER (0..255)                        OPTIONAL,   -- 20 cb_count
    tbSize              L1KPM-Uint32                            OPTIONAL,   -- 29 tb_size
    pduLen              L1KPM-Uint32                            OPTIONAL,   -- 30 pdu_len
    pduOffset           L1KPM-Uint32                            OPTIONAL,   -- 79 pdu_offset
    targetCodeRate      INTEGER (0..65535)                      OPTIONAL,   -- 31 target_code_rate
    newDataIndicator    INTEGER (0..1)                          OPTIONAL,   -- 32 new_data_indicator
    harqProcessId       INTEGER (0..31)                         OPTIONAL,   -- 41 harq_process_id
    rvIndex             INTEGER (0..3)                          OPTIONAL,   -- 42 rv_index

    -- PUSCH path: signal quality
    rsrp                L1KPM-Float32                           OPTIONAL,   -- 18 rsrp
    rssi                L1KPM-Float32                           OPTIONAL,   -- 21 rssi
    noiseVar            L1KPM-Float32                           OPTIONAL,   -- 19 noise_var
    sinr                L1KPM-Float32                           OPTIONAL,   -- 39 sinr
    timingAdvance       L1KPM-Float32                           OPTIONAL,   -- 40 timing_advance
    cfoHz               L1KPM-Float32                           OPTIONAL,   -- 43 cfo_hz

    -- SRS path: measurements
    srsWidebandSnr      L1KPM-Float32                           OPTIONAL,   -- 55 srs_wideband_snr
    srsSignalEnergy     L1KPM-Float32                           OPTIONAL,   -- 56 srs_signal_energy
    srsNoiseEnergy      L1KPM-Float32                           OPTIONAL,   -- 57 srs_noise_energy
    srsToa              L1KPM-Float32                           OPTIONAL,   -- 58 srs_toa
    srsHdAntFlag        INTEGER (0..1)                          OPTIONAL,   -- 59 srs_hd_ant_flag
    srsScCorr           L1KPM-Complex32                         OPTIONAL,   -- 60 srs_sc_corr
    srsCsCorrRatioDb    L1KPM-Float32                           OPTIONAL,   -- 61 srs_cs_corr_ratio_db

    -- SRS path: location of the blobs of the UE
    srsHestNPrbGrps     INTEGER (0..275)                        OPTIONAL,   -- 46 srs_hest_n_prb_grps
    srsHestOffset       L1KPM-Uint32                            OPTIONAL,   -- 47 srs_hest_offset
    srsHestSize         L1KPM-Uint32                            OPTIONAL,   -- 48 srs_hest_size
    srsRbSnrOffset      L1KPM-Uint32                            OPTIONAL,   -- 50 srs_rb_snr_offset
    srsRbSnrSize        L1KPM-Uint32                            OPTIONAL,   -- 51 srs_rb_snr_size
    srsNValidPrg        INTEGER (0..275)                        OPTIONAL,   -- 76 srs_n_valid_prg
    srsPrgSize          INTEGER (1..275)                        OPTIONAL,   -- 77 srs_prg_size

    -- SRS path: configuration of the SRS resource
    srsAntPorts         INTEGER (1..8)                          OPTIONAL,   -- 62 srs_ant_ports
    srsNSyms            INTEGER (1..14)                         OPTIONAL,   -- 63 srs_n_syms
    srsNRepetitions     INTEGER (1..14)                         OPTIONAL,   -- 64 srs_n_repetitions
    srsCombSize         INTEGER (2..8)                          OPTIONAL,   -- 65 srs_comb_size
    srsCombOffset       INTEGER (0..7)                          OPTIONAL,   -- 66 srs_comb_offset
    srsStartSym         INTEGER (0..13)                         OPTIONAL,   -- 67 srs_start_sym
    srsCyclicShift      INTEGER (0..11)                         OPTIONAL,   -- 68 srs_cyclic_shift
    srsFreqPosition     INTEGER (0..67)                         OPTIONAL,   -- 69 srs_freq_position
    srsFreqShift        INTEGER (0..268)                        OPTIONAL,   -- 70 srs_freq_shift
    srsFreqHopping      INTEGER (0..3)                          OPTIONAL,   -- 71 srs_freq_hopping
    srsResourceType     INTEGER (0..2)                          OPTIONAL,   -- 72 srs_resource_type
    srsTSrs             INTEGER (1..2560)                       OPTIONAL,   -- 73 srs_t_srs
    srsTOffset          INTEGER (0..2559)                       OPTIONAL,   -- 74 srs_t_offset
    srsUsage            INTEGER (0..15)                         OPTIONAL,   -- 75 srs_usage
    ...
}

-- **************************************************************
-- SUBSCRIPTION DATA IE
-- **************************************************************

L1KPM-SubscriptionData ::= SEQUENCE {
    cellFilter      SEQUENCE (SIZE (1..maxL1KPMCells)) OF L1KPM-PhysCellId
                                                                OPTIONAL,   -- cell_filter
    ueFilter        SEQUENCE (SIZE (1..maxL1KPMFilterUes)) OF L1KPM-UeRef
                                                                OPTIONAL,   -- ue_filter
    ...
}

L1KPM-UeRef ::= SEQUENCE {
    cellId          L1KPM-PhysCellId                            OPTIONAL,   -- cell_id
    rnti            L1KPM-Rnti,                                             -- rnti
    ...
}

-- **************************************************************
-- CONTROL DATA IE: actionData of the dApp control action
-- **************************************************************

L1KPM-ControlData ::= CHOICE {
    setSubscriptionData     L1KPM-SubscriptionData,         -- control identifier 1
    ...
}

-- **************************************************************
-- RAN FUNCTION DATA IE: ranFunctionData of the RAN function definition
-- **************************************************************

L1KPM-RanFunctionData ::= SEQUENCE {
    name            OCTET STRING (SIZE (1..32)),
    version         INTEGER (0..65535),
    description     OCTET STRING (SIZE (1..128)),
    streams         SEQUENCE (SIZE (1..maxL1KPMStreams)) OF L1KPM-StreamDescriptor,
    ...
}

L1KPM-StreamDescriptor ::= SEQUENCE {
    telemetryId     L1KPM-TelemetryId,                                      -- telemetryIdentifier
    streamId        OCTET STRING (SIZE (1..64)),                            -- stream_id
    dataType        OCTET STRING (SIZE (1..32)),                            -- data_type
    description     OCTET STRING (SIZE (1..256))                OPTIONAL,   -- description
    status          L1KPM-StreamStatus,                                     -- status
    delivery        L1KPM-Delivery                              OPTIONAL,   -- delivery_method
    ...
}

-- No extension marker: a receiver shall not read an unknown value as "available".
L1KPM-StreamStatus ::= ENUMERATED { available, unavailable }

L1KPM-Delivery ::= CHOICE {
    sharedMemory    L1KPM-SharedMemoryInfo,                 -- transport_type "shared_memory"
    protocolData    L1KPM-InlineInfo,                       -- transport_type "protocolData"
    ...
}

L1KPM-SharedMemoryInfo ::= SEQUENCE {
    memoryKey       OCTET STRING (SIZE (1..63)),                            -- memory_key
    memorySizeBytes L1KPM-Uint32,                                           -- memory_size_bytes
    numRows         INTEGER (1..65535),                                     -- num_rows
    ...
}

L1KPM-InlineInfo ::= SEQUENCE {
    keyword         OCTET STRING (SIZE (1..64)),                            -- keyword
    ...
}

END
-- ASN1STOP
```
