// https://github.com/h4tr3d/avcpp/blob/master/example/api2-samples/api2-decode-encode-video.cpp

#include <iostream>

#include "av.h"
#include "avutils.h"
#include "codeccontext.h"
#include "formatcontext.h"

int main(int argc, char **argv) {
    if (argc < 3) return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    std::string uri {argv[1]};
    std::string out {argv[2]};

    std::error_code ec;

    // INPUT
    av::FormatContext ictx;
    ssize_t      videoStream = -1;
    av::VideoDecoderContext vdec;
    av::Stream      vst;

    int count = 0;

    ictx.openInput(uri, ec);
    if (ec) {
        std::cerr << "Can't open input\n";
        return 1;
    }

    ictx.findStreamInfo();

    for (size_t i = 0; i < ictx.streamsCount(); ++i) {
        auto st = ictx.stream(i);
        if (st.mediaType() == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            vst = st;
            break;
        }
    }

    if (vst.isNull()) { std::cerr << "Video stream not found\n"; return 1; }

    if (vst.isValid()) {
        vdec = av::VideoDecoderContext(vst);
        vdec.setRefCountedFrames(true);

        vdec.open(av::Codec(), ec);
        if (ec) {
            std::cerr << "Can't open codec\n";
            return 1;
        }
    }


    // OUTPUT
    av::OutputFormat  ofrmt;
    av::FormatContext octx;

    ofrmt.setFormat(std::string(), out);
    octx.setFormat(ofrmt);

    av::Codec               ocodec  = findEncodingCodec(ofrmt);
    av::VideoEncoderContext encoder {ocodec};

    // Settings
    encoder.setWidth(vdec.width());
    encoder.setHeight(vdec.height());
    if (vdec.pixelFormat() > -1)
        encoder.setPixelFormat(vdec.pixelFormat());
    encoder.setTimeBase(av::Rational{1, 1000});
    encoder.setBitRate(vdec.bitRate());

    encoder.open(av::Codec(), ec);
    if (ec) {
        std::cerr << "Can't opent encodec\n";
        return 1;
    }

    av::Stream ost = octx.addStream(encoder);
    ost.setFrameRate(vst.frameRate());

    octx.openOutput(out, ec);
    if (ec) {
        std::cerr << "Can't open output\n";
        return 1;
    }

    octx.dump();
    octx.writeHeader();
    octx.flush();


    //
    // PROCESS
    //
    while (true) {

        // READING
        av::Packet pkt = ictx.readPacket(ec);
        if (ec) {
            std::clog << "Packet reading error: " << ec << ", " << ec.message() << std::endl;
            break;
        }

        bool flushDecoder = false;
        // !EOF
        if (pkt) {
            if (pkt.streamIndex() != videoStream) {
                continue;
            }

            std::clog << "Read packet: pts=" << pkt.pts() << ", dts=" << pkt.dts() 
                << " / " << pkt.pts().seconds() << " / " << pkt.timeBase() << " / st: " 
                << pkt.streamIndex() << std::endl;

        } else {
            flushDecoder = true;
        }

        do {
            // DECODING
            auto frame = vdec.decode(pkt, ec);

            count++;
            //if (count > 200)
            //    break;

            bool flushEncoder = false;

            if (ec) {
                std::cerr << "Decoding error: " << ec << std::endl;
                return 1;
            } else if (!frame) {
                //cerr << "Empty frame\n";
                //flushDecoder = false;
                //continue;

                if (flushDecoder) {
                    flushEncoder = true;
                }
            }

            if (frame) {
                std::clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " 
                    << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" 
                    << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() 
                    << " / type: " << frame.pictureType()  << std::endl;

                // Change timebase
                frame.setTimeBase(encoder.timeBase());
                frame.setStreamIndex(0);
                frame.setPictureType();

                std::clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " 
                    << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" 
                    << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() 
                    << " / type: " << frame.pictureType()  << std::endl;
            }

            if (frame || flushEncoder) {
                do {
                    // Encode
                    av::Packet opkt = frame ? encoder.encode(frame, ec) : encoder.encode(ec);
                    if (ec) {
                        std::cerr << "Encoding error: " << ec << std::endl;
                        return 1;
                    } else if (!opkt) {
                        //cerr << "Empty packet\n";
                        //continue;
                        break;
                    }

                    // Only one output stream
                    opkt.setStreamIndex(0);

                    std::clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() 
                        << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() 
                        << " / st: " << opkt.streamIndex() << std::endl;

                    octx.writePacket(opkt, ec);
                    if (ec) {
                        std::cerr << "Error write packet: " << ec << ", " << ec.message() << std::endl;
                        return 1;
                    }
                } while (flushEncoder);
            }

            if (flushEncoder)
                break;

        } while (flushDecoder);

        if (flushDecoder)
            break;
    }

    octx.writeTrailer();
}
