// WebSocket binary frame v2 protocol types

// Frame header layout (little-endian):
// [0]     uint8  - version (must be 2)
// [1]     uint8  - frame_type
// [2-3]   uint16 - channel_id
// [4-7]   uint32 - width
// [8-11]  uint32 - height
// [12-15] uint32 - data_length
// [16+]   bytes  - image data (RGBA or JPEG)

export const FRAME_HEADER_SIZE = 16

export const FrameVersion = {
  V2: 2,
} as const

export const FrameType = {
  RGBA: 0,
  JPEG: 1,
  PNG: 2,
} as const

export type FrameTypeValue = (typeof FrameType)[keyof typeof FrameType]

export interface BinaryFrame {
  version: number
  frameType: FrameTypeValue
  channelId: number
  width: number
  height: number
  imageData: Uint8Array<ArrayBuffer>
}

export interface InputEvent {
  type: 'mousedown' | 'mouseup' | 'mousemove' | 'wheel' | 'keydown' | 'keyup'
  channelId: number
  x?: number
  y?: number
  button?: number
  deltaX?: number
  deltaY?: number
  key?: string
  modifiers?: {
    ctrl: boolean
    shift: boolean
    alt: boolean
    meta: boolean
  }
}

export type FrameHandler = (frame: BinaryFrame) => void
