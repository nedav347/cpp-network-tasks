#include "sniffer.h"
#include "pcap_structures.h"

#include <cerrno>
#include <chrono>
#include <iostream>


#ifdef WIN32
#    include <mstcpip.h>
#    include <iphlpapi.h>
#    include <ws2ipdef.h>
const auto IFNAMSIZ = 16;
#else
#    include <sys/ioctl.h>
#    include <sys/socket.h>
#    include <linux/if.h>
#    include <linux/if_packet.h>
#endif

#if !defined(WIN32)
// *nix only.
struct ifreq get_ifr(const std::string &if_name, int sock)
{
    struct ifreq ifr = {0};
    std::copy(if_name.begin(), if_name.end(), ifr.ifr_name);

    if (-1 == ioctl(sock, SIOCGIFINDEX, &ifr))
    {
        throw std::runtime_error(std::string("Unable to find interface ") + if_name);
    }

    return ifr;
}


int get_if_index(const std::string& if_name, int sock)
{
    const auto if_index = get_ifr(if_name, sock).ifr_ifindex;
    std::cout << "Device index = " << if_index << std::endl;
    return if_index;
}


auto get_if_address(const std::string& if_name, int sock)
{
    struct sockaddr_ll iface_addr =
    {
        .sll_family = AF_PACKET,
        .sll_protocol = htons(ETH_P_ALL),
        .sll_ifindex = get_if_index(if_name, sock),
        // Captured IP packets sent and received by the network interface the
        // specified IP address is associated with.
        //.sin_addr = { .s_addr = *reinterpret_cast<const in_addr_t*>(remote_host->h_addr) }
    };

    return iface_addr;
}
#else
auto get_if_address(const std::string& if_name, int sock)
{
    struct sockaddr_in sa = { .sin_family = PF_INET, .sin_port = 0 };
    inet_pton(AF_INET, if_name.c_str(), &sa.sin_addr);

    return sa;
}

#endif


bool Sniffer::init()
{
    // For Windows socket address must be binded before swtich to promisc mode.
    if (!bind_socket()) return false;
    if (!switch_promisc(true)) return false;
    if (!write_pcap_header()) return false;

    initialized_ = true;
    return true;
}


Sniffer::~Sniffer()
{
    stop_capture();
}


bool Sniffer::bind_socket()
{
    const auto iface_addr = get_if_address(if_name_, sock_);

    if (!sock_)
    {
        std::cerr << "Socket error: " << sock_wrap_.get_last_error_string() << "." << std::endl;
        return false;
    }

    const size_t len = if_name_.size();
    if (IFNAMSIZ <= len)
    {
        std::cerr << "Too long interface name!" << std::endl;
        return false;
    }

    /*
    // It's not necessary.
    if (-1 == setsockopt(sock_, SOL_SOCKET, SO_BINDTODEVICE, if_name_.c_str(), len))
    {
        std::cerr << "Interface binding failed: " << sock_wrap_.get_last_error_string() << "." << std::endl;
        return false;
    }*/

    // Bind the socket to the specified IP address.
    if (INVALID_SOCKET == bind(sock_, reinterpret_cast<const struct sockaddr*>(&iface_addr), sizeof(iface_addr)))
    {
        std::cerr << "bind() failed: " << sock_wrap_.get_last_error_string() << "." << std::endl;
        return false;
    }

    return true;
}


bool Sniffer::switch_promisc(bool enabled)
{
#if defined(WIN32)
   // Give us ALL IPv4 packets sent and received to the specific IP address.
    int value = enabled ? RCVALL_ON : RCVALL_OFF;
    DWORD out = 0;

    // The SIO_RCVALL control code enables a socket to receive all IPv4 or IPv6 packets passing through a network interface.
    int rc = WSAIoctl(sock_, SIO_RCVALL, &value, sizeof(value), nullptr, 0, &out, nullptr, nullptr);

    if (INVALID_SOCKET == rc)
    {
        std::cerr << "Ioctl() failed: " << sock_wrap_.get_last_error_string() << "." << std::endl;
        return false;
    }
#else
    auto ifr = get_ifr(if_name_, sock_);

    if (-1 == ioctl(sock_, SIOCGIFFLAGS, &ifr))
    {
        std::cerr << "Unable to get interface flags!" << std::endl;
        return false;
    }

    if (enabled) ifr.ifr_flags |= IFF_PROMISC;
    else ifr.ifr_flags &= ~IFF_PROMISC;

    if (-1 == ioctl(sock_, SIOCSIFFLAGS, &ifr))
    {
        std::cerr << "Unable to set promisc mode!" << std::endl;
        return false;
    }
#endif

   return true;
}


bool Sniffer::write_pcap_header()
{
	if (!of_)
    {
		std::cerr << "\"" << pcap_filename_ << "\"" << "failed [" << errno << "]." << std::endl;
		return false;
	}

    of_.exceptions(std::ifstream::failbit);

    try
    {
        // Disable buffering.
        of_.rdbuf()->pubsetbuf(0, 0);

        // Create a PCAP file header.
        struct pcap_file_header hdr;

        // Write the PCAP file header to our capture file.
        of_.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    }
    catch (const std::ios_base::failure& fail)
    {
        std::cerr << fail.what() << std::endl;
        return false;
    }

    return true;
}


bool Sniffer::capture()
{
    // First 14 bytes are a fake ethernet header with IPv4 as the protocol.
    // `char` type is using for the compatibility with Windows.
    char buffer[BUFFER_SIZE_HDR + BUFFER_SIZE_PKT] = {0};
    buffer[BUFFER_OFFSET_ETH + 12] = 0x08;
    struct pcap_sf_pkthdr* pkt = reinterpret_cast<struct pcap_sf_pkthdr*>(buffer);

    // Reuse this on each loop to calculate a high resolution timestamp.
    std::uint64_t ft_64;

    // Read the next packet, blocking forever.
    int rc = recv(sock_, buffer + BUFFER_OFFSET_IP, BUFFER_SIZE_IP, 0);

    if (INVALID_SOCKET == rc)
    {
        std::cerr << "recv() failed: " << sock_wrap_.get_last_error_string() << std::endl;
        return false;
    }

    // End of file for some strange reason, so stop reading packets.
    if (!rc) return false;

    std::cout << rc << " bytes received..." << std::endl;
    // Calculate timestamp for this packet.
    ft_64 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ft_64 -= EPOCH_BIAS;
    time_t ctime = (time_t) (ft_64 / UNITS_PER_SEC);
    uint32_t ms = ft_64 % UNITS_PER_SEC;

    // Set out PCAP packet header fields.
    pkt->ts.tv_sec = ctime;
    pkt->ts.tv_usec = ms;
    pkt->caplen = rc + BUFFER_SIZE_ETH;
    pkt->len = rc + BUFFER_SIZE_ETH;

    // Output our packet data and header in a single write.
    of_.write(reinterpret_cast<const char*>(buffer), rc + BUFFER_SIZE_ETH + BUFFER_SIZE_HDR);
    of_.flush();

    return true;
}


bool Sniffer::start_capture()
{
    if (started_ || !initialized_) return false;
    started_ = true;

    if (!switch_promisc(true)) return false;
    std::cout << "Starting capture on interface " << if_name_ << std::endl;

    while (started_)
    {
        if (!capture()) return false;
    }

    return true;
}


bool Sniffer::stop_capture()
{
    if (!started_) return false;
    started_ = false;
    switch_promisc(false);

    return true;
}

