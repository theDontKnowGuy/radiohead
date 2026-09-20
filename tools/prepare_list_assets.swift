// Native PNG renderer for the list-surface recipe.  AppKit is already used by
// rasterize_svg.swift, so this keeps asset generation dependency-free on macOS.
import AppKit
import Foundation

struct Surface {
    let name: String
    let width: Int
    let height: Int
    let radius: CGFloat
    let fill: (CGFloat, CGFloat, CGFloat, CGFloat)
    let border: (CGFloat, CGFloat, CGFloat, CGFloat)
}

func color(_ rgba: (CGFloat, CGFloat, CGFloat, CGFloat)) -> NSColor {
    NSColor(calibratedRed: rgba.0 / 255, green: rgba.1 / 255, blue: rgba.2 / 255, alpha: rgba.3 / 255)
}

func image(for surface: Surface) -> NSImage {
    let scale: CGFloat = 4
    let highSize = NSSize(width: CGFloat(surface.width) * scale, height: CGFloat(surface.height) * scale)
    let high = NSImage(size: highSize)
    high.lockFocus()
    NSGraphicsContext.current?.imageInterpolation = .high
    // The surface reaches the native asset edge. This keeps every list gap an
    // exact two pixels; an inset here made the focused first row look farther
    // away from the next row than the remaining pairs.
    let path = NSBezierPath(roundedRect: NSRect(x: 0, y: 0,
                                                 width: highSize.width,
                                                 height: highSize.height),
                            xRadius: surface.radius * scale, yRadius: surface.radius * scale)
    color(surface.fill).setFill()
    path.fill()
    color(surface.border).setStroke()
    path.lineWidth = scale
    path.stroke()
    high.unlockFocus()

    let result = NSImage(size: NSSize(width: surface.width, height: surface.height))
    result.lockFocus()
    NSGraphicsContext.current?.imageInterpolation = .high
    high.draw(in: NSRect(x: 0, y: 0, width: surface.width, height: surface.height),
              from: NSRect(origin: .zero, size: highSize), operation: .sourceOver, fraction: 1)
    result.unlockFocus()
    return result
}

guard CommandLine.arguments.count == 2 else { fatalError("usage: prepare_list_assets.swift OUTPUT") }
let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let surfaces = [
    // Matching edge weight prevents the selected first card's bright lower
    // border from optically creating a larger first-row gap on the TFT.
    Surface(name: "card", width: 248, height: 46, radius: 7, fill: (16, 36, 60, 112), border: (77, 147, 220, 210)),
    Surface(name: "card_focused", width: 248, height: 46, radius: 7, fill: (8, 123, 234, 196), border: (134, 214, 255, 210)),
    Surface(name: "card_wide", width: 304, height: 42, radius: 6, fill: (16, 36, 60, 92), border: (77, 147, 220, 180)),
    Surface(name: "card_wide_focused", width: 304, height: 42, radius: 6, fill: (8, 123, 234, 196), border: (134, 214, 255, 245)),
    Surface(name: "pager", width: 50, height: 44, radius: 8, fill: (16, 36, 60, 102), border: (77, 147, 220, 190)),
    Surface(name: "pager_active", width: 50, height: 44, radius: 8, fill: (8, 123, 234, 196), border: (134, 214, 255, 245)),
    Surface(name: "rail", width: 62, height: 196, radius: 0, fill: (6, 25, 51, 166), border: (51, 115, 185, 130)),
    Surface(name: "pager_thumb_1", width: 44, height: 84, radius: 6, fill: (8, 123, 234, 120), border: (134, 214, 255, 215)),
    Surface(name: "pager_thumb_2", width: 44, height: 42, radius: 6, fill: (8, 123, 234, 120), border: (134, 214, 255, 215)),
    Surface(name: "pager_thumb_3", width: 44, height: 28, radius: 6, fill: (8, 123, 234, 120), border: (134, 214, 255, 215)),
]
for surface in surfaces {
    let image = image(for: surface)
    guard let tiff = image.tiffRepresentation,
          let bitmap = NSBitmapImageRep(data: tiff),
          let png = bitmap.representation(using: .png, properties: [:]) else {
        fatalError("Unable to encode \(surface.name)")
    }
    try png.write(to: output.appendingPathComponent("\(surface.name).png"))
}
