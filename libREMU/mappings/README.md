#  S1, S2, S3 DRAM mapping files
Following https://github.com/CMU-SAFARI/ramulator, users can specify how to map the physical address to the DRAM hierarchy, such as channel/rank/bank/row/column.
We give three mapping files for LPDDR4 (8GB, 128-bit):

- **S1**: [LPDDR4_RoRaBaChCo.map](./LPDDR4_RoRaBaChCo.map) (Default).
- **S2**: [LPDDR4_row_interleaving_16.map](./LPDDR4_row_interleaving_16.map).
- **S3**: [LPDDR4_channel_XOR_16.map](./LPDDR4_channel_XOR_16.map).

  
[1]Kim Y, Yang W, Mutlu O. Ramulator: A fast and extensible DRAM simulator[J]. IEEE Computer architecture letters, 2015, 15(1): 45-49.
