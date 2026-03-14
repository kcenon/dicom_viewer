// Singleton WebSocket manager with binary frame v2 parsing and auto-reconnect

import {
  FRAME_HEADER_SIZE,
  FrameType,
  type BinaryFrame,
  type FrameHandler,
  type InputEvent,
} from '@/types/websocket'

const RECONNECT_BASE_DELAY_MS = 1000
const RECONNECT_MAX_DELAY_MS = 30000
const RECONNECT_MAX_ATTEMPTS = 10

type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error'

class WebSocketManager {
  private ws: WebSocket | null = null
  private url: string | null = null
  private reconnectAttempts = 0
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null
  private frameHandlers = new Map<number, Set<FrameHandler>>()
  private statusListeners = new Set<(status: ConnectionStatus) => void>()
  private status: ConnectionStatus = 'disconnected'

  connect(wsUrl: string): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      return
    }
    this.url = wsUrl
    this.reconnectAttempts = 0
    this.openConnection()
  }

  disconnect(): void {
    this.url = null
    this.clearReconnectTimer()
    if (this.ws) {
      this.ws.onclose = null
      this.ws.close()
      this.ws = null
    }
    this.setStatus('disconnected')
  }

  sendInputEvent(event: InputEvent): void {
    if (this.ws?.readyState !== WebSocket.OPEN) {
      return
    }
    this.ws.send(JSON.stringify(event))
  }

  onFrame(channelId: number, handler: FrameHandler): () => void {
    if (!this.frameHandlers.has(channelId)) {
      this.frameHandlers.set(channelId, new Set())
    }
    this.frameHandlers.get(channelId)!.add(handler)
    return () => {
      this.frameHandlers.get(channelId)?.delete(handler)
    }
  }

  onStatusChange(listener: (status: ConnectionStatus) => void): () => void {
    this.statusListeners.add(listener)
    listener(this.status)
    return () => {
      this.statusListeners.delete(listener)
    }
  }

  private openConnection(): void {
    if (!this.url) return
    this.setStatus('connecting')
    this.ws = new WebSocket(this.url)
    this.ws.binaryType = 'arraybuffer'

    this.ws.onopen = () => {
      this.reconnectAttempts = 0
      this.setStatus('connected')
    }

    this.ws.onmessage = (event: MessageEvent) => {
      if (event.data instanceof ArrayBuffer) {
        this.handleBinaryMessage(event.data)
      }
    }

    this.ws.onerror = () => {
      this.setStatus('error')
    }

    this.ws.onclose = () => {
      this.ws = null
      if (this.url) {
        this.scheduleReconnect()
      }
    }
  }

  private handleBinaryMessage(buffer: ArrayBuffer): void {
    if (buffer.byteLength < FRAME_HEADER_SIZE) {
      return
    }

    const view = new DataView(buffer)
    const version = view.getUint8(0)
    const frameType = view.getUint8(1)
    const channelId = view.getUint16(2, true) // little-endian
    const width = view.getUint32(4, true)
    const height = view.getUint32(8, true)
    const dataLength = view.getUint32(12, true)

    if (version !== 2) {
      console.warn(`[wsManager] Unsupported frame version: ${version}`)
      return
    }

    if (buffer.byteLength < FRAME_HEADER_SIZE + dataLength) {
      return
    }

    const isValidFrameType = (t: number): t is BinaryFrame['frameType'] =>
      t === FrameType.RGBA || t === FrameType.JPEG || t === FrameType.PNG

    if (!isValidFrameType(frameType)) {
      console.warn(`[wsManager] Unknown frame type: ${frameType}`)
      return
    }

    const imageData = new Uint8Array(buffer, FRAME_HEADER_SIZE, dataLength)

    const frame: BinaryFrame = {
      version,
      frameType,
      channelId,
      width,
      height,
      imageData,
    }

    const handlers = this.frameHandlers.get(channelId)
    if (handlers) {
      for (const handler of handlers) {
        handler(frame)
      }
    }
  }

  private scheduleReconnect(): void {
    if (this.reconnectAttempts >= RECONNECT_MAX_ATTEMPTS) {
      this.setStatus('error')
      return
    }

    const delay = Math.min(
      RECONNECT_BASE_DELAY_MS * Math.pow(2, this.reconnectAttempts),
      RECONNECT_MAX_DELAY_MS
    )

    this.reconnectAttempts++
    this.setStatus('connecting')

    this.reconnectTimer = setTimeout(() => {
      this.openConnection()
    }, delay)
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = null
    }
  }

  private setStatus(status: ConnectionStatus): void {
    this.status = status
    for (const listener of this.statusListeners) {
      listener(status)
    }
  }
}

// Singleton export
export const wsManager = new WebSocketManager()
