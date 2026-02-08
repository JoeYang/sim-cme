#include "pcap_reader.h"
#include <cstring>

namespace cme::sim {

static constexpr uint32_t PCAP_MAGIC_MICROSEC  = 0xA1B2C3D4;
static constexpr uint32_t PCAP_MAGIC_NANOSEC   = 0xA1B23C4D;
static constexpr size_t   ETHERNET_HEADER_LEN  = 14;
static constexpr size_t   UDP_HEADER_LEN       = 8;
static constexpr uint8_t  IP_PROTOCOL_UDP      = 17;
static constexpr uint32_t LINKTYPE_ETHERNET    = 1;

PcapReader::PcapReader(const std::string& path) : path_(path) {}

PcapReader::~PcapReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool PcapReader::open() {
    file_.open(path_, std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }

    // Read global header (24 bytes)
    file_.read(reinterpret_cast<char*>(&global_header_), sizeof(PcapGlobalHeader));
    if (!file_.good()) {
        file_.close();
        return false;
    }

    // Validate magic number
    if (global_header_.magic_number == PCAP_MAGIC_MICROSEC) {
        is_nanosecond_ = false;
    } else if (global_header_.magic_number == PCAP_MAGIC_NANOSEC) {
        is_nanosecond_ = true;
    } else {
        file_.close();
        return false;
    }

    // We only support Ethernet link type
    if (global_header_.network != LINKTYPE_ETHERNET) {
        file_.close();
        return false;
    }

    is_open_ = true;
    return true;
}

bool PcapReader::isOpen() const {
    return is_open_;
}

bool PcapReader::readNext(PcapPacket& packet) {
    if (!is_open_) return false;

    // Read packet header (16 bytes)
    PcapPacketHeader pkt_hdr{};
    file_.read(reinterpret_cast<char*>(&pkt_hdr), sizeof(PcapPacketHeader));
    if (!file_.good()) {
        return false;
    }

    // Compute timestamp in microseconds
    if (is_nanosecond_) {
        packet.timestamp_us = static_cast<uint64_t>(pkt_hdr.ts_sec) * 1000000ULL +
                              pkt_hdr.ts_usec / 1000;
    } else {
        packet.timestamp_us = static_cast<uint64_t>(pkt_hdr.ts_sec) * 1000000ULL +
                              pkt_hdr.ts_usec;
    }

    // Read raw packet data
    std::vector<char> raw(pkt_hdr.incl_len);
    file_.read(raw.data(), pkt_hdr.incl_len);
    if (!file_.good()) {
        return false;
    }

    // Strip L2/L3/L4 headers to get UDP payload
    int payload_offset = stripHeaders(raw.data(), raw.size());
    if (payload_offset < 0) {
        // Not a UDP packet -- skip and try the next one
        return readNext(packet);
    }

    size_t payload_len = raw.size() - static_cast<size_t>(payload_offset);
    packet.data.assign(raw.data() + payload_offset, raw.data() + payload_offset + payload_len);
    return true;
}

void PcapReader::reset() {
    if (file_.is_open()) {
        file_.clear();
        // Seek past global header
        file_.seekg(sizeof(PcapGlobalHeader), std::ios::beg);
    }
}

std::vector<PcapPacket> PcapReader::readAll() {
    std::vector<PcapPacket> packets;
    PcapPacket pkt;
    while (readNext(pkt)) {
        packets.push_back(std::move(pkt));
    }
    return packets;
}

int PcapReader::stripHeaders(const char* data, size_t len) {
    // Need at least Ethernet header
    if (len < ETHERNET_HEADER_LEN) return -1;

    // Check EtherType at offset 12-13 (big-endian)
    uint16_t ether_type = (static_cast<uint8_t>(data[12]) << 8) |
                           static_cast<uint8_t>(data[13]);

    // Handle 802.1Q VLAN tag (EtherType 0x8100)
    size_t eth_header_len = ETHERNET_HEADER_LEN;
    if (ether_type == 0x8100) {
        eth_header_len += 4; // 4 bytes VLAN tag
        if (len < eth_header_len) return -1;
        ether_type = (static_cast<uint8_t>(data[eth_header_len - 2]) << 8) |
                      static_cast<uint8_t>(data[eth_header_len - 1]);
    }

    // Must be IPv4 (0x0800)
    if (ether_type != 0x0800) return -1;

    // Parse IPv4 header
    const uint8_t* ip = reinterpret_cast<const uint8_t*>(data + eth_header_len);
    if (len < eth_header_len + 20) return -1;

    uint8_t ip_version = (ip[0] >> 4) & 0x0F;
    if (ip_version != 4) return -1;

    uint8_t ip_ihl = ip[0] & 0x0F; // header length in 32-bit words
    size_t ip_header_len = static_cast<size_t>(ip_ihl) * 4;
    if (ip_header_len < 20 || len < eth_header_len + ip_header_len) return -1;

    // Check protocol field -- must be UDP (17)
    uint8_t protocol = ip[9];
    if (protocol != IP_PROTOCOL_UDP) return -1;

    // Parse UDP header (8 bytes minimum)
    size_t udp_start = eth_header_len + ip_header_len;
    if (len < udp_start + UDP_HEADER_LEN) return -1;

    // Return offset to UDP payload
    return static_cast<int>(udp_start + UDP_HEADER_LEN);
}

} // namespace cme::sim
