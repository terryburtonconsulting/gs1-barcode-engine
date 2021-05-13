﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace gs1encoders_dotnet
{

    public enum symbology : short
    {
        gs1_encoder_sNONE = -1,         // none defined
        gs1_encoder_sRSS14,             // RSS-14
        gs1_encoder_sRSS14T,            // RSS-14 Truncated
        gs1_encoder_sRSS14S,            // RSS-14 Stacked
        gs1_encoder_sRSS14SO,           // RSS-14 Stacked Omnidirectional
        gs1_encoder_sRSSLIM,            // RSS Limited
        gs1_encoder_sRSSEXP,            // RSS Expanded
        gs1_encoder_sUPCA,              // UPC-A
        gs1_encoder_sUPCE,              // UPC-E
        gs1_encoder_sEAN13,             // EAN-13
        gs1_encoder_sEAN8,              // EAN-8
        gs1_encoder_sUCC128_CCA,        // UCC/EAN-128 with CC-A or CC-B
        gs1_encoder_sUCC128_CCC,        // UCC/EAN-128 with CC-C
        gs1_encoder_sNUMSYMS,           // Number of symbologies
    };

    public class GS1Encoder
    {

        private IntPtr ctx;
        private const String gs1_dll = "gs1encoders.dll";

        /*
         *  Functions imported from gs1encoders.dll
         *
         */
        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_init", CallingConvention = CallingConvention.Cdecl)]
        private static extern System.IntPtr gs1_encoder_init();

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getVersion", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr gs1_encoder_getVersion(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getErrMsg", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr gs1_encoder_getErrMsg(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getSym", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getSym(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setSym", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setSym(IntPtr ctx, int sym);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getPixMult", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getPixMult(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setPixMult", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setPixMult(IntPtr ctx, int pixMult);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getXundercut", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getXundercut(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setXundercut", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setXundercut(IntPtr ctx, int Xundercut);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getYundercut", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getYundercut(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setYundercut", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setYundercut(IntPtr ctx, int Yundercut);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getSepHt", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getSepHt(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setSepHt", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setSepHt(IntPtr ctx, int sepHt);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getSegWidth", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getSegWidth(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setSegWidth", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setSegWidth(IntPtr ctx, int segWidth);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getLinHeight", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getLinHeight(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setLinHeight", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setLinHeight(IntPtr ctx, int linHeight);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getFileInputFlag", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_getFileInputFlag(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setFileInputFlag", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setFileInputFlag(IntPtr ctx, bool fileInputFlag);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getDataStr", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr gs1_encoder_getDataStr(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setDataStr", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setDataStr(IntPtr ctx, string dataStr);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getDataFile", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr gs1_encoder_getDataFile(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setDataFile", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setDataFile(IntPtr ctx, string dataFile);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getOutFile", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr gs1_encoder_getOutFile(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setOutFile", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setOutFile(IntPtr ctx, string outFile);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getBmp", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_getBmp(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_setBmp", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_setBmp(IntPtr ctx, bool bmp);
        
        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_encode", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool gs1_encoder_encode(IntPtr ctx);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_getBuffer", CallingConvention = CallingConvention.Cdecl)]
        private static extern int gs1_encoder_getBuffer(IntPtr ctx, ref IntPtr buf);

        [DllImport(gs1_dll, EntryPoint = "gs1_encoder_free", CallingConvention = CallingConvention.Cdecl)]
        private static extern void gs1_encoder_free(IntPtr ctx);

        /*
         *  Methods to provide an OO wrapper around the imported functional interface and to perform marshalling
         *
         */
        public GS1Encoder()
        {
            ctx = gs1_encoder_init();
            if (ctx == IntPtr.Zero)
                throw new GS1EncoderGeneralException("Failed to initalise GS1 Encoder library");
        }      

        public string GetVersion()
        {
            return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gs1_encoder_getVersion(ctx));
        }

        public string GetErrMsg()
        {
            return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gs1_encoder_getErrMsg(ctx));
        }

        public int GetSym()
        {
            return gs1_encoder_getSym(ctx);
        }

        public void SetSym(int sym)
        {
            if (!gs1_encoder_setSym(ctx, sym))
                throw new GS1EncoderParameterException(GetErrMsg());            
        }

        public int GetPixMult()
        {
            return gs1_encoder_getPixMult(ctx);
        }

        public void SetPixMult(int pixMult)
        {
            if (!gs1_encoder_setPixMult(ctx, pixMult))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public int GetXundercut()
        {
            return gs1_encoder_getXundercut(ctx);
        }

        public void SetXundercut(int Xundercut)
        {
            if (!gs1_encoder_setXundercut(ctx, Xundercut))
                throw new GS1EncoderParameterException(GetErrMsg());
        }
        public int GetYundercut()
        {
            return gs1_encoder_getYundercut(ctx);
        }

        public void SetYundercut(int Yundercut)
        {
            if (!gs1_encoder_setYundercut(ctx, Yundercut))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public int GetSepHt()
        {
            return gs1_encoder_getSepHt(ctx);
        }

        public void SetSepHt(int sepHt)
        {
            if (!gs1_encoder_setSepHt(ctx, sepHt))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public int GetSegWidth()
        {
            return gs1_encoder_getSegWidth(ctx);
        }

        public void SetSegWidth(int segWidth)
        {
            if (!gs1_encoder_setSegWidth(ctx, segWidth))
                throw new GS1EncoderParameterException(GetErrMsg());
        }
        
        public int GetLinHeight()
        {
            return gs1_encoder_getLinHeight(ctx);
        }

        public void SetLinHeight(int linHeight)
        {
            if (!gs1_encoder_setLinHeight(ctx, linHeight))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public bool GetFileInputFlag()
        {
            return gs1_encoder_getFileInputFlag(ctx);
        }

        public void SetFileInputFlag(bool fileInputFlag)
        {
            if (!gs1_encoder_setFileInputFlag(ctx, fileInputFlag))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public string GetDataStr()
        {
            return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gs1_encoder_getDataStr(ctx));
        }

        public void SetDataStr(string dataStr)
        {
            if (!gs1_encoder_setDataStr(ctx, dataStr))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public string GetDataFile()
        {
            return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gs1_encoder_getDataFile(ctx));
        }

        public void SetDataFile(string dataFile)
        {
            if (!gs1_encoder_setDataFile(ctx, dataFile))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public bool GetBmp()
        {
            return gs1_encoder_getBmp(ctx);
        }

        public void SetBmp(bool bmp)
        {
            if (!gs1_encoder_setBmp(ctx, bmp))
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public string GetOutFile()
        {
            return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gs1_encoder_getOutFile(ctx));
        }

        public void SetOutFile(string outFile)
        {
            if (!gs1_encoder_setOutFile(ctx, outFile))           
                throw new GS1EncoderParameterException(GetErrMsg());
        }

        public bool Encode()
        {
            return gs1_encoder_encode(ctx);
        }
        public byte[] GetBuffer()
        {

            IntPtr buf = new IntPtr();
            int size = gs1_encoder_getBuffer(ctx, ref buf);

            if (size == 0)
                return new byte[0];
            
            byte[] data = new byte[size];
            Marshal.Copy(buf, data, 0, size);
            return data;        

        }

        ~GS1Encoder()
        {
            gs1_encoder_free(ctx);
        }

    }


    public class GS1EncoderGeneralException : Exception
    {
        public GS1EncoderGeneralException(string message)
           : base(message)
        {
        }
    }

    public class GS1EncoderParameterException : Exception
    {
        public GS1EncoderParameterException(string message)
           : base(message)
        {
        }
    }

    public class GS1EncoderEncodeException : Exception
    {
        public GS1EncoderEncodeException(string message)
           : base(message)
        {
        }
    }

}
