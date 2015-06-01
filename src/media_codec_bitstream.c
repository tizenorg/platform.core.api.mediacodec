/*
* Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
 *
 */
#include <stdio.h>
#include <dlog.h>
#include <media_codec_bitstream.h>

void mc_init_bits(mc_bitstream_t *stream, unsigned char *data, int size)
{
    stream->data = data;
    stream->numBytes = size;
    stream->bitcnt = 32;
    stream->bytePos = 0;
    stream->dataBitPos = 0;
}

short mc_show_bits(mc_bitstream_t *stream, unsigned char nbits, unsigned int *pulOutData)
{
    unsigned char *bits;
    unsigned int dataBitPos = stream->dataBitPos;
    unsigned int bitcnt = stream->bitcnt;
    unsigned int dataBytePos;
    unsigned int i;

    if (nbits > (32 - bitcnt))
    {
        dataBytePos = dataBitPos >> 3;
        bitcnt = dataBitPos & 7;
        if (dataBytePos > stream->numBytes - 4)
        {
            stream->buffer = 0;
            for (i = 0; i < stream->numBytes - dataBytePos; i++)
            {
                stream->buffer |= stream->data[dataBytePos + i];
                stream->buffer <<= 8;
            }
            stream->buffer <<= 8 * (3 - i);
        }
        else
        {
            bits = &stream->data[dataBytePos];
            stream->buffer = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
        }
        stream->bitcnt = bitcnt;
    }
    bitcnt += nbits;

    *pulOutData = (stream->buffer >> (32 - bitcnt)) & mask[(unsigned short)nbits];

    return 0;
}

short mc_read_bits( mc_bitstream_t *stream, unsigned char nbits, unsigned int *pulOutData )
{
    unsigned char *bits;
    unsigned int dataBitPos = stream->dataBitPos;
    unsigned int bitcnt = stream->bitcnt;
    unsigned int dataBytePos;

    if ((dataBitPos + nbits) > (stream->numBytes << 3))
    {
        *pulOutData = 0;
        return -1;
    }

    if (nbits > (32 - bitcnt))
    {
        dataBytePos = dataBitPos >> 3;
        bitcnt = dataBitPos & 7;
        bits = &stream->data[dataBytePos];
        stream->buffer = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
    }


    stream->dataBitPos += nbits;
    stream->bitcnt = (unsigned char)(bitcnt + nbits);


    *pulOutData = (stream->buffer >> (32 - stream->bitcnt)) & mask[(unsigned short)nbits];

    return 0;
}

short mc_byte_align( mc_bitstream_t *stream )
{
    unsigned char *bits;
    unsigned int dataBitPos = stream->dataBitPos;
    unsigned int bitcnt = stream->bitcnt;
    unsigned int dataBytePos;
    unsigned int leftBits;


    leftBits = 8 - (dataBitPos & 0x7);
    if (leftBits == 8)
    {
        if ((dataBitPos + 8) > (unsigned int)(stream->numBytes << 3))
            return (-1);
        dataBitPos += 8;
        bitcnt += 8;
    }
    else
    {
        dataBytePos = dataBitPos >> 3;
        dataBitPos += leftBits;
        bitcnt += leftBits;
    }




    if (bitcnt > 32)
    {
        dataBytePos = dataBitPos >> 3;
        bits = &stream->data[dataBytePos];
        stream->buffer = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
    }


    stream->dataBitPos = dataBitPos;
    stream->bitcnt = bitcnt;

    return 0;
}
unsigned int __mc_bytestream_to_nal( unsigned char* data, int size, unsigned char *nal )
{
    int nal_length = 0;
    unsigned char val, zero_count;
    unsigned char *pNal = data;
    int i = 0;
    int index = 0;

    zero_count = 0;

    val = pNal[index++];
    while (!val)
    {
        if ((zero_count == 2 || zero_count == 3) && val == 1)
            break;
        zero_count++;

        val = pNal[index++];

    }

    nal[nal_length++] = 0;
    nal[nal_length++] = 0;
    nal[nal_length++] = 0;
    nal[nal_length++] = 1;
    zero_count = 0;

    while (1)
    {
        if (index >= size)
            return nal_length;

        val = pNal[index++];

        if (!val)
            zero_count++;
        else {
            if ((zero_count == 2 || zero_count == 3 || zero_count == 4) && (val == 1))
                break;
            else {
                for (i = 0; i<zero_count; i++)
                    nal[nal_length++] = 0;
                nal[nal_length++] = val;
                zero_count = 0;
            }
        }

    }

    return nal_length;
}

int __mc_decode_sps(mc_bitstream_t *pstream, int *width, int *height)
{
    int ret = MC_ERROR_NONE;
    unsigned int tmp = 0;
    unsigned int syntax = 0;

    int profile_idc = 0;


    mc_read_bits(pstream, 8, &tmp);
    mc_read_bits(pstream, 1, &tmp);
    mc_read_bits(pstream, 1, &tmp);
    mc_read_bits(pstream, 1, &tmp);
    mc_read_bits(pstream, 5, &tmp);
    mc_read_bits(pstream, 8, &tmp);

    profile_idc = tmp;

    if (profile_idc > 51)
        ret = MC_INVALID_IN_BUF;

    //TODO parse width, height, etc...

    return ret;
}

int _mc_check_bytestream (media_packet_h pkt, unsigned char *nal, int byte_length, int *width, int *height)
{
    int ret = MC_ERROR_NONE;
    int stacked_length = 0;
    int nal_length = 0;
    unsigned int syntax = 0;
    unsigned char sps[100];
    unsigned char pps[100];
    int sps_size = 0;
    int pps_size = 0;
    int idr = 0;
    unsigned char tmp[1000000];

    mc_bitstream_t pstream;

    while (1)
    {
        nal_length = __mc_bytestream_to_nal( nal + stacked_length, byte_length - stacked_length, tmp);

        mc_init_bits (&pstream, nal + stacked_length, byte_length - stacked_length );
        mc_read_bits (&pstream, 32, &syntax);
        mc_read_bits (&pstream, 8, &syntax);

        switch ( syntax & 0x1F )
        {
            case NAL_SEQUENCE_PARAMETER_SET:
                LOGD("SPS is found");
                if( (ret = __mc_decode_sps ( &pstream, NULL, NULL )) != MC_ERROR_NONE)
                    return ret;
                sps_size = nal_length;
                break;
            case NAL_PICTURE_PARAMETER_SET:
                LOGD("PPS is found");
                pps_size = nal_length;
                break;
            case NAL_SLICE_IDR:
                LOGD ("IDR is found");
                idr++;
                break;
            default:
                LOGD ("%x nal_unit_type is detected");
                break;
        }

        stacked_length += nal_length;

        if ( stacked_length >= byte_length )
            break;
    }

    if ( sps_size > 0 && pps_size > 0 ) {
      memcpy(tmp, sps, sps_size);
      memcpy(tmp + sps_size, pps, pps_size);
      media_packet_set_codec_data( pkt, tmp, sps_size + pps_size);
    }
    else {
        LOGE("doesn't contain codec_data(sps[%d],pps[%d],idr[%d])", sps, pps, idr);
        ret = MC_INVALID_IN_BUF;
    }

    return ret;
}

