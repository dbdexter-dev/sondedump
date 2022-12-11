meta:
  id: m10
  endian: be

seq:
- id: skip_bad
  size: 832/8*1
- id: frames
  size: 832/8
  repeat: eos
  type: frame


types:
  frame:
    seq:
    - id: magic
      size: 4
    - id: type
      type: u1
    - id: data
      type:
        switch-on: type
        cases:
          0x9f: frame_9f
          0x20: frame_20
          _: frame_unk

  frame_unk:
    seq:
    - id: nothing
      size: 0

  frame_20:
    seq:
    - id: unk0
      size: 4
    - id: alt
      size: 3
    - id: dlat
      type: s2
    - id: dlon
      type: s2


  frame_9f:
    seq:
    - id: unk0
      size: 2
    - id: dlat
      type: u2
    - id: dlon
      type: u2
    - id: dalt
      type: u2
    - id: time
      type: u4
    - id: lat
      type: u4
    - id: lon
      type: u4
    - id: alt
      type: u4
    - id: unk1
      size: 6
    - id: week
      type: u2
    - id: pad0
      size: 16
    - id: rh_ref
      size: 3
    - id: rh_val
      size: 3
    - id: pad1
      size: 6
    - id: adc_temp_range
      type: u1
    - id: adc_temp_val
      type: u2
    - id: pad2
      size: 4
    - id: varying2
      type: u2
    - id: varying3
      type: u2
    - id: pad4
      size: 12
    - id: varying4
      type: u2
    - id: pad5
      size: 2
    - id: varying5
      type: u2
    - id: varying6
      type: u2
