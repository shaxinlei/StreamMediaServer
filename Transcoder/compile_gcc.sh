#! /bin/sh
#��򵥵Ļ���FFmpeg���ڴ��д���ӣ��ڴ�ת������----�����б���
#Simplest FFmpeg mem Transcoder----Compile in Shell 
#
#������ Lei Xiaohua
#leixiaohua1020@126.com
#�й���ý��ѧ/���ֵ��Ӽ���
#Communication University of China / Digital TV Technology
#http://blog.csdn.net/leixiaohua1020
#
#compile
gcc simplest_ffmpeg_mem_transcoder.cpp -g -o simplest_ffmpeg_mem_transcoder.out \
-I /usr/local/include -L /usr/local/lib \
-lavcodec -lavformat -lavutil -lavdevice -lavfilter -lpostproc -lswresample -lswscale
