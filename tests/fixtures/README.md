# tests/fixtures — end-to-end test configuration
#
# These CSVs are the repository sample data (etherAdapter/*.csv) with the
# data_proto_id inconsistency fixed so the pipeline can be tested end to end:
#
#   * device_table: only the raw_data device dev_T100_001 (channel 10038),
#     source port 10002, listen port 60382, data_proto_id 100077.
#   * data_header_table: protocol 100077 — frame 73 bytes total (frame_len
#     counts the whole frame), start 0x01, end 0x03, little endian.
#   * data_proto_table: protocol 100077 — one INT16 field "item1" at payload
#     offset 0 (the sample export carries 100078 instead; the design intends
#     the field table to join on the SAME data_proto_id).
#
# Import + run the end-to-end test:
#
#   build\bin\Release\csv2sqlite build\bin\Release\config\etherAdapter.db tests\fixtures --force
#   (or let query_check/csv2sqlite use the path [sqlite].path points at)
#   cd build\bin\Release
#   etherdb_dserver.exe -p 7040            (EtherDB server, separate window)
#   etherAdapter.exe                       (adapter; needs etherAdapter.cfg)
#   device_sim.exe -n 2000 -i 1            (sends 2000 frames, item1 0..1999)
#
# Then verify (item1 should be 0..1999, COUNT = 2000):
#   query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_T100_001"
#   query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001 LIMIT 10"
