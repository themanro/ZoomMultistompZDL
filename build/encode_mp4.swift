// Encode raw frames from stdin as an H.264 MP4 with macOS's own encoder
// (AVFoundation), for the video versions of the README pictures: no ffmpeg.
//
//   swiftc -O build/encode_mp4.swift -o encode_mp4
//   <frames as BGRA bytes, width*height*4 each> | ./encode_mp4 out.mp4 <width> <height> <fps> [Mbit/s]
//
// Width and height must be even. Used by build/make_fx_sheet.py --video and
// build/make_editor_preview.py --video.
import AVFoundation
import Foundation

let a = CommandLine.arguments
guard a.count >= 5, let w = Int(a[2]), let h = Int(a[3]), let fps = Int32(a[4]) else {
    FileHandle.standardError.write("usage: encode_mp4 out.mp4 width height fps [Mbit/s]\n".data(using: .utf8)!)
    exit(2)
}
let mbit = a.count > 5 ? Double(a[5]) ?? 20 : 20
let url = URL(fileURLWithPath: a[1])
try? FileManager.default.removeItem(at: url)

let writer = try AVAssetWriter(outputURL: url, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264,
    AVVideoWidthKey: w,
    AVVideoHeightKey: h,
    AVVideoCompressionPropertiesKey: [
        AVVideoAverageBitRateKey: Int(mbit * 1_000_000),
        AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel,
        AVVideoMaxKeyFrameIntervalKey: Int(fps) * 2,
        AVVideoExpectedSourceFrameRateKey: Int(fps),
    ],
    AVVideoColorPropertiesKey: [
        AVVideoColorPrimariesKey: AVVideoColorPrimaries_ITU_R_709_2,
        AVVideoTransferFunctionKey: AVVideoTransferFunction_ITU_R_709_2,
        AVVideoYCbCrMatrixKey: AVVideoYCbCrMatrix_ITU_R_709_2,
    ],
])
input.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA,
    kCVPixelBufferWidthKey as String: w,
    kCVPixelBufferHeightKey as String: h,
])
writer.add(input)
writer.startWriting()
writer.startSession(atSourceTime: .zero)

let frameBytes = w * h * 4
let stdin = FileHandle.standardInput
var n: Int64 = 0
while true {
    var data = Data()
    while data.count < frameBytes {
        let chunk = stdin.readData(ofLength: frameBytes - data.count)
        if chunk.isEmpty { break }
        data.append(chunk)
    }
    if data.count < frameBytes { break }
    while !input.isReadyForMoreMediaData { usleep(1000) }
    var pb: CVPixelBuffer?
    CVPixelBufferPoolCreatePixelBuffer(nil, adaptor.pixelBufferPool!, &pb)
    guard let buf = pb else { fatalError("no pixel buffer") }
    CVPixelBufferLockBaseAddress(buf, [])
    let dst = CVPixelBufferGetBaseAddress(buf)!.assumingMemoryBound(to: UInt8.self)
    let stride = CVPixelBufferGetBytesPerRow(buf)
    data.withUnsafeBytes { (src: UnsafeRawBufferPointer) in
        for y in 0..<h { memcpy(dst + y * stride, src.baseAddress! + y * w * 4, w * 4) }
    }
    CVPixelBufferUnlockBaseAddress(buf, [])
    adaptor.append(buf, withPresentationTime: CMTime(value: n, timescale: fps))
    n += 1
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()
if writer.status != .completed {
    FileHandle.standardError.write("encode failed: \(String(describing: writer.error))\n".data(using: .utf8)!)
    exit(1)
}
print("\(n) frames -> \(url.path)")
