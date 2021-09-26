meta:
  id: rs41
  endian: le
seq:
  - id: skip_bad
    size: 518*0
  - id: data
    type: raw_frame
    size: 4144/8
    repeat: eos

types:
  raw_frame:
    seq:
    - id: magic
      size: 8
    - id: frame
      type: frame

  frame:
    seq:
      - id: rs_symbols
        size: 48
      - id: is_extended
        type: u1
      - id: subframe
        type: subframe
        repeat: until
        repeat-until: _.type == 0x76

  subframe:
    seq:
      - id: type
        type: u1
      - id: len
        type: u1
      - id: data
        type:
          switch-on: type
          cases:
            0x76: subframe_data_empty
            0x79: subframe_data_info
            0x7a: subframe_data_7a
            0x7b: subframe_data_7b
            0x7c: subframe_data_7c
            0x7e: subframe_data_xdata
            _: subframe_data_generic
      - id: crc
        type: u2
        doc: CRC16, CCITT-FALSE parameters

  subframe_data_7c:
    seq:
      - id: gps_week
        type: u2
      - id: gps_time_ms
        type: u4
      - id: gps_info
        type: u2
        repeat: expr
        repeat-expr: 12

  subframe_data_7b:
    seq:
      - id: data1
        type: u4
      - id: data2
        type: u4
      - id: data3
        type: u4
      - id: unknown
        size: _parent.len - 12


  subframe_data_xdata:
    seq:
      - id: unknown
        type: u1
      - id: asciidata
        type: str
        encoding: ASCII
        size: _parent.len - 1

  subframe_data_7a:
    seq:
      - id: somevalue
        type: subframe_7a_datachunk
        repeat: expr
        repeat-expr: 4
      - id: endpad
        size: _parent.len - 9*4

  subframe_data_generic:
    seq:
      - id: data
        size: _parent.len

  subframe_data_empty:
    seq:
      - id: data
        size: _parent.len

  subframe_data_info:
    seq:
      - id: framecount
        type: u2
      - id: serial_str
        type: str
        encoding: UTF-8
        size: 8
      - id: pad
        size: 12
      - id: max_subframe_seq
        type: u1
      - id: subframe_seq
        type: u1
      - id: subframe_data
        size: 16

  subframe_7a_datachunk:
    seq:
      - id: value
        size: 3
      - id: boundary_1
        size: 3
      - id: boundary_2
        size: 3


