#ifndef __TRACYFILEWRITE_HPP__
#define __TRACYFILEWRITE_HPP__

#include <algorithm>
#include <stdio.h>
#include <string.h>

#include "TracyFileHeader.hpp"
#include "../common/tracy_lz4.hpp"
#include "../common/tracy_lz4hc.hpp"
#include "../common/TracyForceInline.hpp"

namespace tracy
{

class FileWrite
{
public:
    static FileWrite* Open( const char* fn, bool hc = false )
    {
        auto f = fopen( fn, "wb" );
        return f ? new FileWrite( f, hc ) : nullptr;
    }

    ~FileWrite()
    {
        if( m_offset > 0 )
        {
            WriteLz4Block();
        }
        fclose( m_file );

        if( m_stream ) LZ4_freeStream( m_stream );
        if( m_streamHC ) LZ4_freeStreamHC( m_streamHC );
    }

    tracy_force_inline void Write( const void* ptr, size_t size )
    {
        if( m_offset + size <= BufSize )
        {
            WriteSmall( ptr, size );
        }
        else
        {
            WriteBig( ptr, size );
        }
    }

private:
    FileWrite( FILE* f, bool hc )
        : m_stream( nullptr )
        , m_streamHC( nullptr )
        , m_file( f )
        , m_buf( m_bufData[0] )
        , m_second( m_bufData[1] )
        , m_offset( 0 )
    {
        if( hc )
        {
            m_streamHC = LZ4_createStreamHC();
        }
        else
        {
            m_stream = LZ4_createStream();
        }

        fwrite( Lz4Header, 1, sizeof( Lz4Header ), m_file );
    }

    tracy_force_inline void WriteSmall( const void* ptr, size_t size )
    {
        memcpy( m_buf + m_offset, ptr, size );
        m_offset += size;
    }

    void WriteBig( const void* ptr, size_t size )
    {
        auto src = (const char*)ptr;
        while( size > 0 )
        {
            const auto sz = std::min( size, BufSize - m_offset );
            memcpy( m_buf + m_offset, src, sz );
            m_offset += sz;
            src += sz;
            size -= sz;

            if( m_offset == BufSize )
            {
                WriteLz4Block();
            }
        }
    }

    void WriteLz4Block()
    {
        char lz4[LZ4Size];
        uint32_t sz;
        if( m_stream )
        {
            sz = LZ4_compress_fast_continue( m_stream, m_buf, lz4, m_offset, LZ4Size, 1 );
        }
        else
        {
            sz = LZ4_compress_HC_continue( m_streamHC, m_buf, lz4, m_offset, LZ4Size );
        }
        fwrite( &sz, 1, sizeof( sz ), m_file );
        fwrite( lz4, 1, sz, m_file );
        m_offset = 0;
        std::swap( m_buf, m_second );
    }

    enum { BufSize = 64 * 1024 };
    enum { LZ4Size = LZ4_COMPRESSBOUND( BufSize ) };

    LZ4_stream_t* m_stream;
    LZ4_streamHC_t* m_streamHC;
    FILE* m_file;
    char m_bufData[2][BufSize];
    char* m_buf;
    char* m_second;
    size_t m_offset;
};

}

#endif
