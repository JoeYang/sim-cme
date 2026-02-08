// Compatibility header for Boost.Asio vs standalone Asio
// When ASIO_STANDALONE is defined (no Boost), we include standalone headers
// and create namespace aliases so the rest of the code can use boost::asio uniformly.
#pragma once

#ifdef ASIO_STANDALONE
    #include <asio.hpp>
    #include <asio/steady_timer.hpp>
    #include <asio/strand.hpp>
    #include <asio/ip/tcp.hpp>
    #include <asio/ip/udp.hpp>
    #include <asio/ip/multicast.hpp>
    #include <asio/read.hpp>
    #include <asio/write.hpp>
    #include <asio/executor_work_guard.hpp>

    // Create boost::asio namespace alias pointing to standalone asio
    namespace boost {
        namespace asio = ::asio;
        namespace system {
            using ::asio::error_code;
        }
    }
#else
    #include <boost/asio.hpp>
    #include <boost/asio/steady_timer.hpp>
    #include <boost/asio/strand.hpp>
    #include <boost/asio/ip/tcp.hpp>
    #include <boost/asio/ip/udp.hpp>
    #include <boost/asio/ip/multicast.hpp>
    #include <boost/asio/read.hpp>
    #include <boost/asio/write.hpp>
    #include <boost/asio/executor_work_guard.hpp>
#endif
