// RemoteViewport renders binary frames from WebSocket onto a canvas element.
// RGBA frames: ImageData -> createImageBitmap (GPU upload off main thread)
// JPEG/PNG frames: Blob -> createImageBitmap (decoded off main thread)
// Rendering is decoupled from network delivery via requestAnimationFrame.

import { useEffect, useRef, useCallback } from 'react'
import { wsManager } from '@/api/wsManager'
import { useViewportStore } from '@/stores/viewportStore'
import { FrameType } from '@/types/websocket'
import type { BinaryFrame } from '@/types/websocket'
import type { InputEvent } from '@/types/websocket'

interface Props {
  channelId: number
  isActive: boolean
  onClick: () => void
}

export function RemoteViewport({ channelId, isActive, onClick }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  // Latest decoded bitmap, consumed by the rAF loop
  const bitmapRef = useRef<ImageBitmap | null>(null)
  const pendingBitmapRef = useRef<ImageBitmap | null>(null)
  const rafIdRef = useRef<number>(0)
  // Track canvas size for input coordinate normalisation
  const canvasSizeRef = useRef<{ width: number; height: number }>({ width: 0, height: 0 })
  const setFrameResolution = useViewportStore((s) => s.setFrameResolution)

  // Decode an incoming frame into an ImageBitmap off the main thread
  const handleFrame = useCallback(async (frame: BinaryFrame) => {
    let bitmap: ImageBitmap

    if (frame.frameType === FrameType.RGBA) {
      // Raw RGBA: wrap in ImageData then createImageBitmap
      const clamped = new Uint8ClampedArray(
        frame.imageData.buffer,
        frame.imageData.byteOffset,
        frame.imageData.byteLength
      )
      const imgData = new ImageData(clamped, frame.width, frame.height)
      bitmap = await createImageBitmap(imgData)
    } else {
      // JPEG or PNG: wrap in Blob, browser decodes in a worker
      const mime = frame.frameType === FrameType.JPEG ? 'image/jpeg' : 'image/png'
      const blob = new Blob([frame.imageData], { type: mime })
      bitmap = await createImageBitmap(blob)
    }

    // Swap: discard any pending bitmap that was never rendered
    pendingBitmapRef.current?.close()
    pendingBitmapRef.current = bitmap
  }, [])

  // rAF loop: draw the latest bitmap each frame
  const renderLoop = useCallback(() => {
    const canvas = canvasRef.current
    if (!canvas) return

    const pending = pendingBitmapRef.current
    if (pending !== null) {
      const ctx = canvas.getContext('2d')
      if (ctx) {
        // Resize canvas to match frame dimensions if needed
        if (canvas.width !== pending.width || canvas.height !== pending.height) {
          canvas.width = pending.width
          canvas.height = pending.height
          canvasSizeRef.current = { width: pending.width, height: pending.height }
          setFrameResolution(channelId, pending.width, pending.height)
        }
        ctx.drawImage(pending, 0, 0)

        // Release previous bitmap after draw
        bitmapRef.current?.close()
        bitmapRef.current = pending
        pendingBitmapRef.current = null
      }
    }

    rafIdRef.current = requestAnimationFrame(renderLoop)
  }, [channelId, setFrameResolution])

  // Subscribe to frames for this channel
  useEffect(() => {
    const unsubscribe = wsManager.onFrame(channelId, (frame) => {
      void handleFrame(frame)
    })
    return unsubscribe
  }, [channelId, handleFrame])

  // Start/stop rAF loop
  useEffect(() => {
    rafIdRef.current = requestAnimationFrame(renderLoop)
    return () => {
      cancelAnimationFrame(rafIdRef.current)
      bitmapRef.current?.close()
      bitmapRef.current = null
      pendingBitmapRef.current?.close()
      pendingBitmapRef.current = null
    }
  }, [renderLoop])

  // Forward mouse/keyboard input events to the server
  const buildInputEvent = useCallback(
    (base: Omit<InputEvent, 'channelId'>): InputEvent => ({
      ...base,
      channelId,
    }),
    [channelId]
  )

  const getModifiers = (e: React.MouseEvent | React.KeyboardEvent | React.WheelEvent) => ({
    ctrl: e.ctrlKey,
    shift: e.shiftKey,
    alt: e.altKey,
    meta: e.metaKey,
  })

  const getCanvasCoords = (e: React.MouseEvent) => {
    const rect = canvasRef.current?.getBoundingClientRect()
    if (!rect) return { x: 0, y: 0 }
    return { x: e.clientX - rect.left, y: e.clientY - rect.top }
  }

  const handleMouseDown = (e: React.MouseEvent) => {
    onClick()
    const { x, y } = getCanvasCoords(e)
    wsManager.sendInputEvent(buildInputEvent({ type: 'mousedown', x, y, button: e.button, modifiers: getModifiers(e) }))
  }

  const handleMouseUp = (e: React.MouseEvent) => {
    const { x, y } = getCanvasCoords(e)
    wsManager.sendInputEvent(buildInputEvent({ type: 'mouseup', x, y, button: e.button, modifiers: getModifiers(e) }))
  }

  const handleMouseMove = (e: React.MouseEvent) => {
    if (e.buttons === 0) return
    const { x, y } = getCanvasCoords(e)
    wsManager.sendInputEvent(buildInputEvent({ type: 'mousemove', x, y, modifiers: getModifiers(e) }))
  }

  const handleWheel = (e: React.WheelEvent) => {
    const { x, y } = getCanvasCoords(e)
    wsManager.sendInputEvent(buildInputEvent({ type: 'wheel', x, y, deltaX: e.deltaX, deltaY: e.deltaY, modifiers: getModifiers(e) }))
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    wsManager.sendInputEvent(buildInputEvent({ type: 'keydown', key: e.key, modifiers: getModifiers(e) }))
  }

  const handleKeyUp = (e: React.KeyboardEvent) => {
    wsManager.sendInputEvent(buildInputEvent({ type: 'keyup', key: e.key, modifiers: getModifiers(e) }))
  }

  return (
    <div
      style={{
        position: 'relative',
        width: '100%',
        height: '100%',
        background: '#000',
        outline: isActive ? '2px solid #2196f3' : '1px solid #333',
        boxSizing: 'border-box',
        cursor: 'crosshair',
        overflow: 'hidden',
      }}
      tabIndex={0}
      onKeyDown={handleKeyDown}
      onKeyUp={handleKeyUp}
    >
      <canvas
        ref={canvasRef}
        style={{ display: 'block', width: '100%', height: '100%', objectFit: 'contain' }}
        onMouseDown={handleMouseDown}
        onMouseUp={handleMouseUp}
        onMouseMove={handleMouseMove}
        onWheel={handleWheel}
      />
    </div>
  )
}
