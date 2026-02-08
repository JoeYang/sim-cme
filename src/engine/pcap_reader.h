#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace cme::sim {

struct PcapPacket {
    uint64_t timestamp_us;   // microseconds from pcap header
    std::vector<char> data;  // UDP payload (after stripping Ethernet/IP/UDP headers)
};

class PcapReader {
public:
    explicit PcapReader(const std::string& path);
    ~PcapReader();

    bool open();
    bool isOpen() const;

    // Read next packet, returns false at EOF
    bool readNext(PcapPacket& packet);

    // Reset to beginning
    void reset();

    // Read all packets
    std::vector<PcapPacket> readAll();

private:
    std::string path_;
    std::ifstream file_;
    bool is_open_ = false;
    bool is_nanosecond_ = false; // pcap vs pcapng timestamp resolution

    // Pcap global header
    struct PcapGlobalHeader {
        uint32_t magic_number;
        uint16_t version_major;
        uint16_t version_minor;
        int32_t thiszone;
        uint32_t sigfigs;
        uint32_t snaplen;
        uint32_t network; // link-layer type
    };

    // Pcap packet header
    struct PcapPacketHeader {
        uint32_t ts_sec;
        uint32_t ts_usec; // or ts_nsec for nanosecond pcaps
        uint32_t incl_len;
        uint32_t orig_len;
    };

    PcapGlobalHeader global_header_{};

    // Strip Ethernet (14B) + IP (20B min) + UDP (8B) headers
    // Returns offset to UDP payload, or -1 if not UDP
    int stripHeaders(const char* data, size_t len);
};

} // namespace cme::sim
