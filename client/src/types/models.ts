// Domain model types shared across stores

export type ViewportLayout = '1x1' | '1x2' | '2x2' | '2x3' | '3x3'

export type SegmentationTool =
  | 'none'
  | 'brush'
  | 'eraser'
  | 'levelTracing'
  | 'scissors'
  | 'hollow'

export type MeasurementTool =
  | 'none'
  | 'length'
  | 'angle'
  | 'ellipse'
  | 'rectangle'
  | 'arrow'

export interface SegmentationLabel {
  id: number
  name: string
  color: [number, number, number, number] // RGBA
  visible: boolean
  opacity: number
}

export interface Measurement {
  id: string
  type: MeasurementTool
  channelId: number
  points: Array<{ x: number; y: number }>
  value?: number
  unit?: string
  label?: string
}

export interface ViewportChannel {
  channelId: number
  viewportIndex: number
  studyInstanceUid?: string
  seriesInstanceUid?: string
}

export interface QualityMetrics {
  fps: number
  latencyMs: number
  droppedFrames: number
  bandwidthKbps: number
}

export interface FlowQuantificationData {
  phaseIndex: number
  totalPhases: number
  forwardFlowMl: number
  reverseFlowMl: number
  netFlowMl: number
  peakVelocityMs: number
}
