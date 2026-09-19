import AppKit
import Foundation

guard CommandLine.arguments.count == 4,
      let side = Int(CommandLine.arguments[3]), side > 0 else {
    fatalError("Usage: rasterize_svg.swift INPUT.svg OUTPUT.png SIZE")
}

let input = URL(fileURLWithPath: CommandLine.arguments[1])
let output = URL(fileURLWithPath: CommandLine.arguments[2])
guard let image = NSImage(contentsOf: input) else {
    fatalError("Could not load SVG: \(input.path)")
}
guard let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: side,
                                    pixelsHigh: side, bitsPerSample: 8,
                                    samplesPerPixel: 4, hasAlpha: true,
                                    isPlanar: false, colorSpaceName: .deviceRGB,
                                    bytesPerRow: 0, bitsPerPixel: 0),
      let context = NSGraphicsContext(bitmapImageRep: bitmap) else {
    fatalError("Could not create RGBA bitmap")
}

NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = context
context.cgContext.clear(CGRect(x: 0, y: 0, width: side, height: side))
image.draw(in: CGRect(x: 0, y: 0, width: side, height: side),
           from: CGRect(origin: .zero, size: image.size), operation: .sourceOver,
           fraction: 1.0, respectFlipped: false, hints: nil)
NSGraphicsContext.restoreGraphicsState()

guard let png = bitmap.representation(using: .png, properties: [:]) else {
    fatalError("Could not encode PNG")
}
try png.write(to: output)
