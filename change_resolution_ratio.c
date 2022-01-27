#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>



#include "libavutil/imgutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavutil/parseutils.h"
#include "libswscale/swscale.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"



int change_resolution_ratio(const char *pSrcFileName, const char *pDstFileName, int dstWidth, int dstHeight)
{
	int ret = 0;
	
	AVFormatContext *pSrcFmtCtx = NULL;			//源格式上下文
	AVCodecContext *pSrcCodecCtx = NULL;		//源编码上下文
	AVCodec *pSrcCodec = NULL;					//源编码器
	AVPacket srcPacket;							//源包
	memset(&srcPacket, 0, sizeof(AVPacket));
	
	AVFormatContext *pDstFmtCtx = NULL;			//目标格式上下文
	AVOutputFormat *pOutputFmt = NULL;			//输出格式
	AVStream *pDstVideoStream = NULL;			//目标视频流
	AVCodec *pDstCodec = NULL;					//目标编码器
	AVCodecContext *pDstCodecCtx = NULL;		//目标编码上下文
	AVPacket dstPacket;							//目标包
	memset(&dstPacket, 0, sizeof(AVPacket));
	
	AVFrame *pSrcFrame = NULL;					//源帧
	AVFrame *pDstFrame = NULL;					//目标帧
	unsigned char *pDstFrameBuffer = NULL;		//目标帧缓存区
	struct SwsContext *pSwsCtx = NULL;			//转换上下文
	
	
	//注册所有组件
	av_register_all();
	avcodec_register_all();
	
	/*-----------------------------输入----------------------------------*/
	
	//打开输入文件，并获得格式上下文
	if((ret = avformat_open_input(&pSrcFmtCtx, pSrcFileName, NULL, NULL)) < 0)
	{
		fprintf(stderr, "avformat_open_input in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//读取媒体文件的数据包以获取流信息,因为有的格式可能没有头信息
	if((ret = avformat_find_stream_info(pSrcFmtCtx, NULL)) < 0) 
	{
		fprintf(stderr, "avformat_find_stream_info in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//找到视频流
	int srcVideoStreamIndx = -1;
	int i = 0;
	for(i = 0; i < pSrcFmtCtx->nb_streams; i++)
	{
		if(pSrcFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			srcVideoStreamIndx = i;
			break;
		}
	}
	
	//获取编码器上下文
	pSrcCodecCtx = pSrcFmtCtx->streams[srcVideoStreamIndx]->codec;
	pSrcCodec = avcodec_find_decoder(pSrcCodecCtx->codec_id);
	if(pSrcCodec == NULL)
	{
		fprintf(stderr, "avcodec_find_decoder in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//打开解码器
	if((ret = avcodec_open2(pSrcCodecCtx, pSrcCodec, NULL)) < 0)
	{
		fprintf(stderr, "avcodec_open2 in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//输出信息
	//av_dump_format(pSrcFmtCtx, 0, pSrcFileName, 0);
	
	//初始化输入包
	av_init_packet(&srcPacket);
	
	/*------------------------------输出-------------------------------------*/
	
	//创建格式上下文
	avformat_alloc_output_context2(&pDstFmtCtx, NULL, NULL, pDstFileName);
	if(pDstFmtCtx == NULL)
	{
		fprintf(stderr, "avformat_alloc_output_context2 in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	pOutputFmt = pDstFmtCtx->oformat;
	
	//打开文件
	if((ret = avio_open(&pDstFmtCtx->pb, pDstFileName, AVIO_FLAG_READ_WRITE)) < 0)
	{
		fprintf(stderr, "avio_open in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//创建流
	for(i = 0; i < pSrcFmtCtx->nb_streams; i++)
	{
		//创建流
		AVStream *pInStream = pSrcFmtCtx->streams[i];
		AVStream *pOutStream = NULL;
		pOutStream = avformat_new_stream(pDstFmtCtx, NULL);
		if(pOutStream == NULL)
		{
			fprintf(stderr, "avformat_new_stream in %s:%d error\n", __FILE__, __LINE__);
			goto out;
		}
		
		//如果是视频流
		if(pSrcFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			pDstVideoStream = pOutStream;
			
			//初始化视频码流
			pDstVideoStream->time_base.num = pSrcFmtCtx->streams[srcVideoStreamIndx]->avg_frame_rate.den;
			pDstVideoStream->time_base.den = pSrcFmtCtx->streams[srcVideoStreamIndx]->avg_frame_rate.num;
		
			continue;
		}
		
		//复制编码参数
		if(avcodec_copy_context(pOutStream->codec, pInStream->codec) < 0)
		{
			fprintf(stderr, "avcodec_copy_context in %s:%d error\n", __FILE__, __LINE__);
			goto out;
		}

		pOutStream->codec->codec_tag = 0;

		if (pDstFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
			pOutStream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	
	//编码器上下文设置参数
	pDstCodecCtx = pDstVideoStream->codec;  
    pDstCodecCtx->codec_id = pOutputFmt->video_codec;  
    pDstCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;  
    pDstCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;  
    pDstCodecCtx->width = dstWidth;  
    pDstCodecCtx->height = dstHeight;  
    pDstCodecCtx->time_base.num = pSrcFmtCtx->streams[srcVideoStreamIndx]->avg_frame_rate.den;  
    pDstCodecCtx->time_base.den = pSrcFmtCtx->streams[srcVideoStreamIndx]->avg_frame_rate.num;  
    pDstCodecCtx->bit_rate = ((float)pSrcCodecCtx->bit_rate / pSrcCodecCtx->width / pSrcCodecCtx->height 
							* dstWidth * dstHeight);  
    pDstCodecCtx->gop_size = pSrcCodecCtx->gop_size;  
	
	if(pDstCodecCtx->codec_id == AV_CODEC_ID_H264)  
    {  
        pDstCodecCtx->qmin = pSrcCodecCtx->qmin;  
        pDstCodecCtx->qmax = pSrcCodecCtx->qmax;  
        pDstCodecCtx->qcompress = pSrcCodecCtx->qcompress;  
    }  
    if (pDstCodecCtx->codec_id == AV_CODEC_ID_MPEG2VIDEO)  
        pDstCodecCtx->max_b_frames = pSrcCodecCtx->max_b_frames;  
    if (pDstCodecCtx->codec_id == AV_CODEC_ID_MPEG1VIDEO)  
        pDstCodecCtx->mb_decision = pSrcCodecCtx->mb_decision;  
	
	
	//寻找编码器并打开编码器
	pDstCodec = avcodec_find_encoder(pDstCodecCtx->codec_id);  
    if(!pDstCodec)  
    {  
		fprintf(stderr, "avcodec_find_encoder in %s:%d error\n", __FILE__, __LINE__);
		goto out;
    }  
    if(avcodec_open2(pDstCodecCtx, pDstCodec, NULL) < 0)  
    {  
		fprintf(stderr, "avcodec_open2 in %s:%d error\n", __FILE__, __LINE__);
		goto out;
    }  
	
	//输出格式信息  
    //av_dump_format(pDstFmtCtx, 0, pDstFileName, 1); 
	
	//写文件头
	if(ret = avformat_write_header(pDstFmtCtx, NULL) < 0)
	{
		fprintf(stderr, "avformat_write_header in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//初始化输出包
	av_init_packet(&dstPacket);

	
	/*--------------------------------转换-----------------------------------*/
	
	//创建解码后的帧和转换后的帧
	pSrcFrame = av_frame_alloc();
	pDstFrame = av_frame_alloc();
	if(pSrcFrame == NULL || pDstFrame == NULL)
	{
		fprintf(stderr, "avcodec_find_decoder in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//创建输出帧的缓存区
	unsigned long long DstFrameBuffSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, dstWidth, dstHeight, 1);
	pDstFrameBuffer = (unsigned char *)av_malloc(DstFrameBuffSize);
	av_image_fill_arrays(pDstFrame->data, pDstFrame->linesize, pDstFrameBuffer,
		AV_PIX_FMT_YUV420P, dstWidth, dstHeight, 1);
	
	//创建转换上下文结构体
	pSwsCtx = sws_getContext(pSrcCodecCtx->width, pSrcCodecCtx->height, pSrcCodecCtx->pix_fmt,
		dstWidth, dstHeight, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
	if(pSwsCtx == NULL)
	{
		fprintf(stderr, "avcodec_find_decoder in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	
	//设置时间结构体
	struct timeval timeOld;
	gettimeofday(&timeOld, NULL);
	struct timeval timeNew;
	memset(&timeNew, 0, sizeof(struct timeval));
	
	//获取源文件的大小
	int srcFd = 0;
	struct stat srcStat;
	memset(&srcStat, 0, sizeof(struct stat));
	srcFd = open(pSrcFileName, O_RDONLY);
	if(srcFd == 0)
	{
		fprintf(stderr, "open in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	ret = fstat(srcFd, &srcStat);
	if(ret < 0)
	{
		fprintf(stderr, "open in %s:%d error\n", __FILE__, __LINE__);
		goto out;
	}
	close(srcFd);
	
	//循环转换
	int decodeGotPicture = 0;
	int encodeGotPicture = 0;
	unsigned long long frameNumber = 0;		//帧编号
	unsigned long long readSize = 0;		//读到的大小
	while(av_read_frame(pSrcFmtCtx, &srcPacket) >= 0)
	{
		readSize += srcPacket.size;
		
		//每隔一秒打印一次读到的大小
		gettimeofday(&timeNew, NULL);
		if(timeNew.tv_sec - timeOld.tv_sec > 1)
		{
			printf("readSize = %llu/%llu......%0.2f%%\n", readSize, (unsigned long long)srcStat.st_size, (float)readSize / srcStat.st_size * 100);
			memcpy(&timeOld, &timeNew, sizeof(struct timeval));
		}
			
		
		decodeGotPicture = 0;
		encodeGotPicture = 0;
		
		//如果为视频流
		if(srcPacket.stream_index == srcVideoStreamIndx)
		{
			//解码
			if((ret = avcodec_decode_video2(pSrcCodecCtx, pSrcFrame, &decodeGotPicture, &srcPacket)) < 0)
			{
				fprintf(stderr, "avcodec_find_decoder in %s:%d error\n", __FILE__, __LINE__);
				goto out;
			}
			if(decodeGotPicture)
			{
				//转换
				sws_scale(pSwsCtx, (const unsigned char* const*)pSrcFrame->data, pSrcFrame->linesize, 0, pSrcCodecCtx->height,
					pDstFrame->data, pDstFrame->linesize);
					

				//编码  
				frameNumber++;
				pDstFrame->pts = frameNumber;
				pDstFrame->format = AV_PIX_FMT_YUV420P;
				pDstFrame->width = dstWidth;
				pDstFrame->height = dstHeight;
				
				int ret = avcodec_encode_video2(pDstCodecCtx, &dstPacket, pDstFrame, &encodeGotPicture);  
				if(ret<0)  
				{  
					fprintf(stderr, "avcodec_encode_video2 in %s:%d error\n", __FILE__, __LINE__);
					goto out; 
				} 

				if(encodeGotPicture)
				{
					dstPacket.stream_index = pDstVideoStream->index;  
					av_packet_rescale_ts(&dstPacket, pDstCodecCtx->time_base, pDstVideoStream->time_base);  
					dstPacket.pos = -1;  
					if(ret = av_interleaved_write_frame(pDstFmtCtx, &dstPacket) < 0)
					{
						fprintf(stderr, "av_interleaved_write_frame in %s:%d error\n", __FILE__, __LINE__);
						goto out;
					}						
				}
			}
			
			av_free_packet(&srcPacket);
			av_free_packet(&dstPacket);
		}
		//如果是其他流
		else
		{
			//转换时间戳
			AVStream *pInStream = pSrcFmtCtx->streams[srcPacket.stream_index];
			AVStream *pOutStream = pDstFmtCtx->streams[srcPacket.stream_index];
			srcPacket.pts = av_rescale_q_rnd(srcPacket.pts, pInStream->time_base, pOutStream->time_base, 
									(enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			srcPacket.dts = av_rescale_q_rnd(srcPacket.dts, pInStream->time_base, pOutStream->time_base, 
									(enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			srcPacket.duration = av_rescale_q(srcPacket.duration, pInStream->time_base, pOutStream->time_base);
			
			//写入数据
			if(ret = av_interleaved_write_frame(pDstFmtCtx, &srcPacket) < 0)
			{
				fprintf(stderr, "av_interleaved_write_frame in %s:%d error\n", __FILE__, __LINE__);
				goto out;
			}		
			
			av_free_packet(&srcPacket);
		}
		
		
	}
	//刷新解码器(可能有的数据还在解码器)
	while(1)
	{
		if((ret = avcodec_decode_video2(pSrcCodecCtx, pSrcFrame, &decodeGotPicture, &srcPacket)) < 0)
		{
			break;
		}
		if (!decodeGotPicture)
		{
			break; 
		}
		
		//转换
		sws_scale(pSwsCtx, (const unsigned char* const*)pSrcFrame->data, pSrcFrame->linesize, 0, pSrcCodecCtx->height,
			pDstFrame->data, pDstFrame->linesize);
		
		
				
		//编码  
		frameNumber ++;
		pDstFrame->pts = frameNumber;
		pDstFrame->format = AV_PIX_FMT_YUV420P;
		pDstFrame->width = dstWidth;
		pDstFrame->height = dstHeight;
		
		int ret = avcodec_encode_video2(pDstCodecCtx, &dstPacket, pDstFrame, &encodeGotPicture);  
		if(ret<0)  
		{  
			fprintf(stderr, "avcodec_encode_video2 in %s:%d error\n", __FILE__, __LINE__);
			goto out; 
		} 

		if(encodeGotPicture)
		{
			//转换时间戳
			dstPacket.stream_index = pDstVideoStream->index;  
			av_packet_rescale_ts(&dstPacket, pDstCodecCtx->time_base, pDstVideoStream->time_base);  
			dstPacket.pos = -1;  
			
			//写入数据
			ret = av_interleaved_write_frame(pDstFmtCtx, &dstPacket);  
			if(ret < 0)
				break;
		}
		
		av_free_packet(&srcPacket);
		av_free_packet(&dstPacket);
	}
	//刷新编码器
	while(1)
	{
		ret = avcodec_encode_video2(pDstCodecCtx, &dstPacket, NULL, &encodeGotPicture);
		if(ret<0)  
		{  
			fprintf(stderr, "avcodec_encode_video2 in %s:%d error\n", __FILE__, __LINE__);
			goto out;  
		} 
		if (!encodeGotPicture)
		{
			break; 
		}
		
		//转换时间戳
		dstPacket.stream_index = pDstVideoStream->index;  
		av_packet_rescale_ts(&dstPacket, pDstCodecCtx->time_base, pDstVideoStream->time_base);  
		dstPacket.pos = -1;  
		
		//写入数据
		ret = av_interleaved_write_frame(pDstFmtCtx, &dstPacket);  
		if(ret < 0)
			break;
		
		av_free_packet(&dstPacket);
	}
	
	//写文件尾
	av_write_trailer(pDstFmtCtx);
	
	//输出格式信息  
    //av_dump_format(pDstFmtCtx, 0, pDstFileName, 1); 
	
	
out:
	if(pSrcFmtCtx)
		avformat_close_input(&pSrcFmtCtx);
	if(pSrcCodecCtx)
		avcodec_close(pSrcCodecCtx);
	if(pSrcFrame)
		av_frame_free(&pSrcFrame);
	if(pDstFrame)
		av_frame_free(&pDstFrame);
	if(pSwsCtx)
		sws_freeContext(pSwsCtx);
	
	
	if(pDstFmtCtx)
	{
		avformat_free_context(pDstFmtCtx);
		if(pDstFmtCtx->pb)
			avio_close(pDstFmtCtx->pb);
	}
	
	if(pDstFrameBuffer)
		av_free(pDstFrameBuffer);

	
	return ret;
}










