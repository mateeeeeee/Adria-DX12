/* ======================================================================================== */
/* FMOD Core API - DSP header file.                                                         */
/* Copyright (c), Firelight Technologies Pty, Ltd. 2004-2020.                               */
/*                                                                                          */
/* Use this header if you are wanting to develop your own DSP plugin to use with FMODs      */
/* dsp system.  With this header you can make your own DSP plugin that FMOD can             */
/* register and use.  See the documentation and examples on how to make a working plugin.   */
/*                                                                                          */
/* For more detail visit:                                                                   */
/* https://fmod.com/resources/documentation-api?version=2.0&page=plugin-api-dsp.html        */
/* =========================================================================================*/

using System;
using System.Text;
using System.Runtime.InteropServices;

namespace FMOD
{
    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_BUFFER_ARRAY
    {
        public int              numbuffers;              /* [r/w] number of buffers */
        public int[]            buffernumchannels;       /* [r/w] array of number of channels for each buffer */
        public CHANNELMASK[]    bufferchannelmask;       /* [r/w] array of channel masks for each buffer */
        public IntPtr[]         buffers;                 /* [r/w] array of buffers */
        public SPEAKERMODE      speakermode;             /* [r/w] speaker mode for all buffers in the array */
    }

    public enum DSP_PROCESS_OPERATION
    {
        PROCESS_PERFORM = 0,               /* Process the incoming audio in 'inbufferarray' and output to 'outbufferarray'. */
        PROCESS_QUERY                      /* The DSP is being queried for the expected output format and whether it needs to process audio or should be bypassed.  The function should return any value other than FMOD_OK if audio can pass through unprocessed. If audio is to be processed, 'outbufferarray' must be filled with the expected output format, channel count and mask. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct COMPLEX
    {
        public float real; /* Real component */
        public float imag; /* Imaginary component */
    }

    public enum DSP_PAN_SURROUND_FLAGS
    {
        DEFAULT = 0,
        ROTATION_NOT_BIASED = 1,
    }


    /*
        DSP callbacks
    */
    public delegate RESULT DSP_CREATECALLBACK                   (ref DSP_STATE dsp_state);
    public delegate RESULT DSP_RELEASECALLBACK                  (ref DSP_STATE dsp_state);
    public delegate RESULT DSP_RESETCALLBACK                    (ref DSP_STATE dsp_state);
    public delegate RESULT DSP_SETPOSITIONCALLBACK              (ref DSP_STATE dsp_state, uint pos);
    public delegate RESULT DSP_READCALLBACK                     (ref DSP_STATE dsp_state, IntPtr inbuffer, IntPtr outbuffer, uint length, int inchannels, ref int outchannels);
    public delegate RESULT DSP_SHOULDIPROCESS_CALLBACK          (ref DSP_STATE dsp_state, bool inputsidle, uint length, CHANNELMASK inmask, int inchannels, SPEAKERMODE speakermode);
    public delegate RESULT DSP_PROCESS_CALLBACK                 (ref DSP_STATE dsp_state, uint length, ref DSP_BUFFER_ARRAY inbufferarray, ref DSP_BUFFER_ARRAY outbufferarray, bool inputsidle, DSP_PROCESS_OPERATION op);
    public delegate RESULT DSP_SETPARAM_FLOAT_CALLBACK          (ref DSP_STATE dsp_state, int index, float value);
    public delegate RESULT DSP_SETPARAM_INT_CALLBACK            (ref DSP_STATE dsp_state, int index, int value);
    public delegate RESULT DSP_SETPARAM_BOOL_CALLBACK           (ref DSP_STATE dsp_state, int index, bool value);
    public delegate RESULT DSP_SETPARAM_DATA_CALLBACK           (ref DSP_STATE dsp_state, int index, IntPtr data, uint length);
    public delegate RESULT DSP_GETPARAM_FLOAT_CALLBACK          (ref DSP_STATE dsp_state, int index, ref float value, IntPtr valuestr);
    public delegate RESULT DSP_GETPARAM_INT_CALLBACK            (ref DSP_STATE dsp_state, int index, ref int value, IntPtr valuestr);
    public delegate RESULT DSP_GETPARAM_BOOL_CALLBACK           (ref DSP_STATE dsp_state, int index, ref bool value, IntPtr valuestr);
    public delegate RESULT DSP_GETPARAM_DATA_CALLBACK           (ref DSP_STATE dsp_state, int index, ref IntPtr data, ref uint length, IntPtr valuestr);
    public delegate RESULT DSP_SYSTEM_REGISTER_CALLBACK         (ref DSP_STATE dsp_state);
    public delegate RESULT DSP_SYSTEM_DEREGISTER_CALLBACK       (ref DSP_STATE dsp_state);
    public delegate RESULT DSP_SYSTEM_MIX_CALLBACK              (ref DSP_STATE dsp_state, int stage);


    /*
        DSP functions
    */
    public delegate IntPtr DSP_ALLOC_FUNC                         (uint size, MEMORY_TYPE type, IntPtr sourcestr);
    public delegate IntPtr DSP_REALLOC_FUNC                       (IntPtr ptr, uint size, MEMORY_TYPE type, IntPtr sourcestr);
    public delegate void   DSP_FREE_FUNC                          (IntPtr ptr, MEMORY_TYPE type, IntPtr sourcestr);
    public delegate void   DSP_LOG_FUNC                           (DEBUG_FLAGS level, IntPtr file, int line, IntPtr function, IntPtr format);
    public delegate RESULT DSP_GETSAMPLERATE_FUNC                 (ref DSP_STATE dsp_state, ref int rate);
    public delegate RESULT DSP_GETBLOCKSIZE_FUNC                  (ref DSP_STATE dsp_state, ref uint blocksize);
    public delegate RESULT DSP_GETSPEAKERMODE_FUNC                (ref DSP_STATE dsp_state, ref int speakermode_mixer, ref int speakermode_output);
    public delegate RESULT DSP_GETCLOCK_FUNC                      (ref DSP_STATE dsp_state, out ulong clock, out uint offset, out uint length);
    public delegate RESULT DSP_GETLISTENERATTRIBUTES_FUNC         (ref DSP_STATE dsp_state, ref int numlisteners, IntPtr attributes);
    public delegate RESULT DSP_GETUSERDATA_FUNC                   (ref DSP_STATE dsp_state, out IntPtr userdata);
    public delegate RESULT DSP_DFT_FFTREAL_FUNC                   (ref DSP_STATE dsp_state, int size, IntPtr signal, IntPtr dft, IntPtr window, int signalhop);
    public delegate RESULT DSP_DFT_IFFTREAL_FUNC                  (ref DSP_STATE dsp_state, int size, IntPtr dft, IntPtr signal, IntPtr window, int signalhop);
    public delegate RESULT DSP_PAN_SUMMONOMATRIX_FUNC             (ref DSP_STATE dsp_state, int sourceSpeakerMode, float lowFrequencyGain, float overallGain, IntPtr matrix);
    public delegate RESULT DSP_PAN_SUMSTEREOMATRIX_FUNC           (ref DSP_STATE dsp_state, int sourceSpeakerMode, float pan, float lowFrequencyGain, float overallGain, int matrixHop, IntPtr matrix);
    public delegate RESULT DSP_PAN_SUMSURROUNDMATRIX_FUNC         (ref DSP_STATE dsp_state, int sourceSpeakerMode, int targetSpeakerMode, float direction, float extent, float rotation, float lowFrequencyGain, float overallGain, int matrixHop, IntPtr matrix, DSP_PAN_SURROUND_FLAGS flags);
    public delegate RESULT DSP_PAN_SUMMONOTOSURROUNDMATRIX_FUNC   (ref DSP_STATE dsp_state, int targetSpeakerMode, float direction, float extent, float lowFrequencyGain, float overallGain, int matrixHop, IntPtr matrix);
    public delegate RESULT DSP_PAN_SUMSTEREOTOSURROUNDMATRIX_FUNC (ref DSP_STATE dsp_state, int targetSpeakerMode, float direction, float extent, float rotation, float lowFrequencyGain, float overallGain, int matrixHop, IntPtr matrix);
    public delegate RESULT DSP_PAN_GETROLLOFFGAIN_FUNC            (ref DSP_STATE dsp_state, DSP_PAN_3D_ROLLOFF_TYPE rolloff, float distance, float mindistance, float maxdistance, out float gain);
    

    public enum DSP_TYPE : int
    {
        UNKNOWN,            /* This unit was created via a non FMOD plugin so has an unknown purpose. */
        MIXER,              /* This unit does nothing but take inputs and mix them together then feed the result to the soundcard unit. */
        OSCILLATOR,         /* This unit generates sine/square/saw/triangle or noise tones. */
        LOWPASS,            /* This unit filters sound using a high quality, resonant lowpass filter algorithm but consumes more CPU time. */
        ITLOWPASS,          /* This unit filters sound using a resonant lowpass filter algorithm that is used in Impulse Tracker, but with limited cutoff range (0 to 8060hz). */
        HIGHPASS,           /* This unit filters sound using a resonant highpass filter algorithm. */
        ECHO,               /* This unit produces an echo on the sound and fades out at the desired rate. */
        FADER,              /* This unit pans and scales the volume of a unit. */
        FLANGE,             /* This unit produces a flange effect on the sound. */
        DISTORTION,         /* This unit distorts the sound. */
        NORMALIZE,          /* This unit normalizes or amplifies the sound to a certain level. */
        LIMITER,            /* This unit limits the sound to a certain level. */
        PARAMEQ,            /* This unit attenuates or amplifies a selected frequency range. */
        PITCHSHIFT,         /* This unit bends the pitch of a sound without changing the speed of playback. */
        CHORUS,             /* This unit produces a chorus effect on the sound. */
        VSTPLUGIN,          /* This unit allows the use of Steinberg VST plugins */
        WINAMPPLUGIN,       /* This unit allows the use of Nullsoft Winamp plugins */
        ITECHO,             /* This unit produces an echo on the sound and fades out at the desired rate as is used in Impulse Tracker. */
        COMPRESSOR,         /* This unit implements dynamic compression (linked multichannel, wideband) */
        SFXREVERB,          /* This unit implements SFX reverb */
        LOWPASS_SIMPLE,     /* This unit filters sound using a simple lowpass with no resonance, but has flexible cutoff and is fast. */
        DELAY,              /* This unit produces different delays on individual channels of the sound. */
        TREMOLO,            /* This unit produces a tremolo / chopper effect on the sound. */
        LADSPAPLUGIN,       /* This unit allows the use of LADSPA standard plugins. */
        SEND,               /* This unit sends a copy of the signal to a return DSP anywhere in the DSP tree. */
        RETURN,             /* This unit receives signals from a number of send DSPs. */
        HIGHPASS_SIMPLE,    /* This unit filters sound using a simple highpass with no resonance, but has flexible cutoff and is fast. */
        PAN,                /* This unit pans the signal, possibly upmixing or downmixing as well. */
        THREE_EQ,           /* This unit is a three-band equalizer. */
        FFT,                /* This unit simply analyzes the signal and provides spectrum information back through getParameter. */
        LOUDNESS_METER,     /* This unit analyzes the loudness and true peak of the signal. */
        ENVELOPEFOLLOWER,   /* This unit tracks the envelope of the input/sidechain signal. Deprecated and will be removed in a future release. */
        CONVOLUTIONREVERB,  /* This unit implements convolution reverb. */
        CHANNELMIX,         /* This unit provides per signal channel gain, and output channel mapping to allow 1 multichannel signal made up of many groups of signals to map to a single output signal. */
        TRANSCEIVER,        /* This unit 'sends' and 'receives' from a selection of up to 32 different slots.  It is like a send/return but it uses global slots rather than returns as the destination.  It also has other features.  Multiple transceivers can receive from a single channel, or multiple transceivers can send to a single channel, or a combination of both. */
        OBJECTPAN,          /* This unit sends the signal to a 3d object encoder like Dolby Atmos. */
        MULTIBAND_EQ,       /* This unit is a flexible five band parametric equalizer. */
        MAX
    }

    public enum DSP_PARAMETER_TYPE
    {
        FLOAT = 0,
        INT,
        BOOL,
        DATA,
        MAX
    }

    public enum DSP_PARAMETER_FLOAT_MAPPING_TYPE
    {
        DSP_PARAMETER_FLOAT_MAPPING_TYPE_LINEAR = 0,          /* Values mapped linearly across range. */
        DSP_PARAMETER_FLOAT_MAPPING_TYPE_AUTO,                /* A mapping is automatically chosen based on range and units.  See remarks. */
        DSP_PARAMETER_FLOAT_MAPPING_TYPE_PIECEWISE_LINEAR,    /* Values mapped in a piecewise linear fashion defined by FMOD_DSP_PARAMETER_DESC_FLOAT::mapping.piecewiselinearmapping. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_FLOAT_MAPPING_PIECEWISE_LINEAR
    {
        public int numpoints;                       /* [w] The number of <position, value> pairs in the piecewise mapping (at least 2). */
        public IntPtr pointparamvalues;             /* [w] The values in the parameter's units for each point */
        public IntPtr pointpositions;               /* [w] The positions along the control's scale (e.g. dial angle) corresponding to each parameter value.  The range of this scale is arbitrary and all positions will be relative to the minimum and maximum values (e.g. [0,1,3] is equivalent to [1,2,4] and [2,4,8]).  If this array is zero, pointparamvalues will be distributed with equal spacing. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_FLOAT_MAPPING
    {
        public DSP_PARAMETER_FLOAT_MAPPING_TYPE type;
        public DSP_PARAMETER_FLOAT_MAPPING_PIECEWISE_LINEAR piecewiselinearmapping;    /* [w] Only required for FMOD_DSP_PARAMETER_FLOAT_MAPPING_TYPE_PIECEWISE_LINEAR type mapping. */
    }


    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_DESC_FLOAT
    {
        public float                     min;                      /* [w] Minimum parameter value. */
        public float                     max;                      /* [w] Maximum parameter value. */
        public float                     defaultval;               /* [w] Default parameter value. */
        public DSP_PARAMETER_FLOAT_MAPPING mapping;           /* [w] How the values are distributed across dials and automation curves (e.g. linearly, exponentially etc). */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_DESC_INT
    {
        public int                       min;                      /* [w] Minimum parameter value. */
        public int                       max;                      /* [w] Maximum parameter value. */
        public int                       defaultval;               /* [w] Default parameter value. */
        public bool                      goestoinf;                /* [w] Whether the last value represents infiniy. */
        public IntPtr                    valuenames;               /* [w] Names for each value.  There should be as many strings as there are possible values (max - min + 1).  Optional. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_DESC_BOOL
    {
        public bool                      defaultval;               /* [w] Default parameter value. */
        public IntPtr                    valuenames;               /* [w] Names for false and true, respectively.  There should be two strings.  Optional. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_DESC_DATA
    {
        public int                       datatype;                 /* [w] The type of data for this parameter.  Use 0 or above for custom types or set to one of the FMOD_DSP_PARAMETER_DATA_TYPE values. */
    }

    [StructLayout(LayoutKind.Explicit)]
    public struct DSP_PARAMETER_DESC_UNION
    {
        [FieldOffset(0)]
        public DSP_PARAMETER_DESC_FLOAT   floatdesc;  /* [w] Struct containing information about the parameter in floating point format.  Use when type is FMOD_DSP_PARAMETER_TYPE_FLOAT. */
        [FieldOffset(0)]
        public DSP_PARAMETER_DESC_INT     intdesc;    /* [w] Struct containing information about the parameter in integer format.  Use when type is FMOD_DSP_PARAMETER_TYPE_INT. */
        [FieldOffset(0)]
        public DSP_PARAMETER_DESC_BOOL    booldesc;   /* [w] Struct containing information about the parameter in boolean format.  Use when type is FMOD_DSP_PARAMETER_TYPE_BOOL. */
        [FieldOffset(0)]
        public DSP_PARAMETER_DESC_DATA    datadesc;   /* [w] Struct containing information about the parameter in data format.  Use when type is FMOD_DSP_PARAMETER_TYPE_DATA. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_DESC
    {
        public DSP_PARAMETER_TYPE   type;            /* [w] Type of this parameter. */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public byte[]               name;            /* [w] Name of the parameter to be displayed (ie "Cutoff frequency"). */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public byte[]               label;           /* [w] Short string to be put next to value to denote the unit type (ie "hz"). */
        public string               description;     /* [w] Description of the parameter to be displayed as a help item / tooltip for this parameter. */

        public DSP_PARAMETER_DESC_UNION desc;
    }

    public enum DSP_PARAMETER_DATA_TYPE
    {
        DSP_PARAMETER_DATA_TYPE_USER = 0,                /* The default data type.  All user data types should be 0 or above. */
        DSP_PARAMETER_DATA_TYPE_OVERALLGAIN = -1,        /* The data type for FMOD_DSP_PARAMETER_OVERALLGAIN parameters.  There should a maximum of one per DSP. */
        DSP_PARAMETER_DATA_TYPE_3DATTRIBUTES = -2,       /* The data type for FMOD_DSP_PARAMETER_3DATTRIBUTES parameters.  There should a maximum of one per DSP. */
        DSP_PARAMETER_DATA_TYPE_SIDECHAIN = -3,          /* The data type for FMOD_DSP_PARAMETER_SIDECHAIN parameters.  There should a maximum of one per DSP. */
        DSP_PARAMETER_DATA_TYPE_FFT = -4,                /* The data type for FMOD_DSP_PARAMETER_FFT parameters.  There should a maximum of one per DSP. */
        DSP_PARAMETER_DATA_TYPE_3DATTRIBUTES_MULTI = -5  /* The data type for FMOD_DSP_PARAMETER_3DATTRIBUTES_MULTI parameters.  There should a maximum of one per DSP. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_OVERALLGAIN
    {
        public float linear_gain;                                  /* [r] The overall linear gain of the effect on the direct signal path */
        public float linear_gain_additive;                         /* [r] Additive gain, for parallel signal paths */
    }
    
    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_3DATTRIBUTES
    {
        public ATTRIBUTES_3D relative;                        /* [w] The position of the sound relative to the listener. */
        public ATTRIBUTES_3D absolute;                        /* [w] The position of the sound in world coordinates. */
    }
    
    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_3DATTRIBUTES_MULTI
    {
        public int            numlisteners;                    /* [w] The number of listeners. */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public ATTRIBUTES_3D[] relative;                       /* [w] The position of the sound relative to the listeners. */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] weight;                                 /* [w] The weighting of the listeners where 0 means listener has no contribution and 1 means full contribution. */
        public ATTRIBUTES_3D absolute;                         /* [w] The position of the sound in world coordinates. */
    }
    
    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_SIDECHAIN
    {
        public int sidechainenable;                               /* [r/w] Whether sidechains are enabled. */
    }
    
    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_PARAMETER_FFT
    {
        public int     length;                                    /* [r] Number of entries in this spectrum window.   Divide this by the output rate to get the hz per entry. */
        public int     numchannels;                               /* [r] Number of channels in spectrum. */
        
        [MarshalAs(UnmanagedType.ByValArray,SizeConst=32)]
        private IntPtr[] spectrum_internal;                           /* [r] Per channel spectrum arrays.  See remarks for more. */

        public float[][] spectrum
        {
            get
            {
                var buffer = new float[numchannels][];
                
                for (int i = 0; i < numchannels; ++i)
                {
                    buffer[i] = new float[length];
                    Marshal.Copy(spectrum_internal[i], buffer[i], 0, length);
                }
                
                return buffer;
            }
        }

        public void getSpectrum(ref float[][] buffer)
        {
            int bufferLength = Math.Min(buffer.Length, numchannels);
            for (int i = 0; i < bufferLength; ++i)
            {
                getSpectrum(i, ref buffer[i]);
            }
        }

        public void getSpectrum(int channel, ref float[] buffer)
        {
            int bufferLength = Math.Min(buffer.Length, length);
            Marshal.Copy(spectrum_internal[channel], buffer, 0, bufferLength);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_DESCRIPTION
    {
        public uint                           pluginsdkversion;   /* [w] The plugin SDK version this plugin is built for.  set to this to FMOD_PLUGIN_SDK_VERSION defined above. */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public char[]                         name;               /* [w] Name of the unit to be displayed in the network. */
        public uint                           version;            /* [w] Plugin writer's version number. */
        public int                            numinputbuffers;    /* [w] Number of input buffers to process.  Use 0 for DSPs that only generate sound and 1 for effects that process incoming sound. */
        public int                            numoutputbuffers;   /* [w] Number of audio output buffers.  Only one output buffer is currently supported. */
        public DSP_CREATECALLBACK             create;             /* [w] Create callback.  This is called when DSP unit is created.  Can be null. */
        public DSP_RELEASECALLBACK            release;            /* [w] Release callback.  This is called just before the unit is freed so the user can do any cleanup needed for the unit.  Can be null. */
        public DSP_RESETCALLBACK              reset;              /* [w] Reset callback.  This is called by the user to reset any history buffers that may need resetting for a filter, when it is to be used or re-used for the first time to its initial clean state.  Use to avoid clicks or artifacts. */
        public DSP_READCALLBACK               read;               /* [w] Read callback.  Processing is done here.  Can be null. */
        public DSP_PROCESS_CALLBACK           process;            /* [w] Process callback.  Can be specified instead of the read callback if any channel format changes occur between input and output.  This also replaces shouldiprocess and should return an error if the effect is to be bypassed.  Can be null. */
        public DSP_SETPOSITIONCALLBACK        setposition;        /* [w] Setposition callback.  This is called if the unit wants to update its position info but not process data.  Can be null. */

        public int                            numparameters;      /* [w] Number of parameters used in this filter.  The user finds this with DSP::getNumParameters */
        public IntPtr                         paramdesc;          /* [w] Variable number of parameter structures. */
        public DSP_SETPARAM_FLOAT_CALLBACK    setparameterfloat;  /* [w] This is called when the user calls DSP.setParameterFloat. Can be null. */
        public DSP_SETPARAM_INT_CALLBACK      setparameterint;    /* [w] This is called when the user calls DSP.setParameterInt.   Can be null. */
        public DSP_SETPARAM_BOOL_CALLBACK     setparameterbool;   /* [w] This is called when the user calls DSP.setParameterBool.  Can be null. */
        public DSP_SETPARAM_DATA_CALLBACK     setparameterdata;   /* [w] This is called when the user calls DSP.setParameterData.  Can be null. */
        public DSP_GETPARAM_FLOAT_CALLBACK    getparameterfloat;  /* [w] This is called when the user calls DSP.getParameterFloat. Can be null. */
        public DSP_GETPARAM_INT_CALLBACK      getparameterint;    /* [w] This is called when the user calls DSP.getParameterInt.   Can be null. */
        public DSP_GETPARAM_BOOL_CALLBACK     getparameterbool;   /* [w] This is called when the user calls DSP.getParameterBool.  Can be null. */
        public DSP_GETPARAM_DATA_CALLBACK     getparameterdata;   /* [w] This is called when the user calls DSP.getParameterData.  Can be null. */
        public DSP_SHOULDIPROCESS_CALLBACK    shouldiprocess;     /* [w] This is called before processing.  You can detect if inputs are idle and return FMOD_OK to process, or any other error code to avoid processing the effect.  Use a count down timer to allow effect tails to process before idling! */
        public IntPtr                         userdata;           /* [w] Optional. Specify 0 to ignore. This is user data to be attached to the DSP unit during creation.  Access via FMOD_DSP_STATE_FUNCTIONS::getuserdata. */

        public DSP_SYSTEM_REGISTER_CALLBACK   sys_register;       /* [w] Register callback.  This is called when DSP unit is loaded/registered.  Useful for 'global'/per system object init for plugin.  Can be null. */
        public DSP_SYSTEM_DEREGISTER_CALLBACK sys_deregister;     /* [w] Deregister callback.  This is called when DSP unit is unloaded/deregistered.  Useful as 'global'/per system object shutdown for plugin.  Can be null. */
        public DSP_SYSTEM_MIX_CALLBACK        sys_mix;            /* [w] System mix stage callback.  This is called when the mixer starts to execute or is just finishing executing.  Useful for 'global'/per system object once a mix update calls for a plugin.  Can be null. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_STATE_DFT_FUNCTIONS
    {
        public DSP_DFT_FFTREAL_FUNC  fftreal;        /* [r] Function for performing an FFT on a real signal. */
        public DSP_DFT_IFFTREAL_FUNC inversefftreal; /* [r] Function for performing an inverse FFT to get a real signal. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_STATE_PAN_FUNCTIONS
    {
        public DSP_PAN_SUMMONOMATRIX_FUNC             summonomatrix;             /* [r] TBD. */
        public DSP_PAN_SUMSTEREOMATRIX_FUNC           sumstereomatrix;           /* [r] TBD. */
        public DSP_PAN_SUMSURROUNDMATRIX_FUNC         sumsurroundmatrix;         /* [r] TBD. */
        public DSP_PAN_SUMMONOTOSURROUNDMATRIX_FUNC   summonotosurroundmatrix;   /* [r] TBD. */
        public DSP_PAN_SUMSTEREOTOSURROUNDMATRIX_FUNC sumstereotosurroundmatrix; /* [r] TBD. */
        public DSP_PAN_GETROLLOFFGAIN_FUNC            getrolloffgain;            /* [r] TBD. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_STATE_FUNCTIONS
    {
        public DSP_ALLOC_FUNC                  alloc;                  /* [r] Memory allocation callback. Use this for all dynamic memory allocation within the plugin. */
        public DSP_REALLOC_FUNC                realloc;                /* [r] Memory reallocation callback. */
        public DSP_FREE_FUNC                   free;                   /* [r] Memory free callback. */
        public DSP_GETSAMPLERATE_FUNC          getsamplerate;          /* [r] Callback for getting the system samplerate. */
        public DSP_GETBLOCKSIZE_FUNC           getblocksize;           /* [r] Callback for getting the system's block size.  DSPs will be requested to process blocks of varying length up to this size.*/
        public IntPtr                          dft;                    /* [r] Struct containing callbacks for performing FFTs and inverse FFTs. */
        public IntPtr                          pan;                    /* [r] Pointer to a structure of callbacks for calculating pan, up-mix and down-mix matrices. */
        public DSP_GETSPEAKERMODE_FUNC         getspeakermode;         /* [r] Callback for getting the system's speaker modes.  One is the mixer's default speaker mode, the other is the output mode the system is downmixing or upmixing to.*/
        public DSP_GETCLOCK_FUNC               getclock;               /* [r] Callback for getting the clock of the current DSP, as well as the subset of the input buffer that contains the signal */
        public DSP_GETLISTENERATTRIBUTES_FUNC  getlistenerattributes;  /* [r] Callback for getting the absolute listener attributes set via the API (returned as left-handed co-ordinates). */
        public DSP_LOG_FUNC                    log;                    /* [r] Function to write to the FMOD logging system. */
        public DSP_GETUSERDATA_FUNC            getuserdata;            /* [r] Function to get the user data attached to this DSP. See FMOD_DSP_DESCRIPTION::userdata. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_STATE
    {
        public IntPtr     instance;            /* [r] Handle to the DSP hand the user created.  Not to be modified.  C++ users cast to FMOD::DSP to use.  */
        public IntPtr     plugindata;          /* [r/w] Plugin writer created data the output author wants to attach to this object. */
        public uint       channelmask;         /* [r] Specifies which speakers the DSP effect is active on */
        public int        source_speakermode;  /* [r] Specifies which speaker mode the signal originated for information purposes, ie in case panning needs to be done differently. */
        public IntPtr     sidechaindata;       /* [r] The mixed result of all incoming sidechains is stored at this pointer address. */
        public int        sidechainchannels;   /* [r] The number of channels of pcm data stored within the sidechain buffer. */
        public IntPtr     functions;           /* [r] Struct containing callbacks for system level functionality. */
        public int        systemobject;        /* [r] FMOD::System object index, relating to the System object that created this DSP. */
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DSP_METERING_INFO
    {
        public int   numsamples;        /* [r] The number of samples considered for this metering info. */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=32)]
        public float[] peaklevel;       /* [r] The peak level per channel. */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=32)]
        public float[] rmslevel;        /* [r] The rms level per channel. */
        public short numchannels;       /* [r] Number of channels. */
    }

    /*
        ==============================================================================================================

        FMOD built in effect parameters.
        Use DSP::setParameter with these enums for the 'index' parameter.

        ==============================================================================================================
    */

    public enum DSP_OSCILLATOR : int
    {
        TYPE,   /* Waveform type.  0 = sine.  1 = square. 2 = sawup. 3 = sawdown. 4 = triangle. 5 = noise.  */
        RATE    /* Frequency of the sinewave in hz.  1.0 to 22000.0.  Default = 220.0. */
    }

    public enum DSP_LOWPASS : int
    {
        CUTOFF,    /* Lowpass cutoff frequency in hz.   1.0 to 22000.0.  Default = 5000.0. */
        RESONANCE  /* Lowpass resonance Q value. 1.0 to 10.0.  Default = 1.0. */
    }

    public enum DSP_ITLOWPASS : int
    {
        CUTOFF,    /* Lowpass cutoff frequency in hz.  1.0 to 22000.0.  Default = 5000.0/ */
        RESONANCE  /* Lowpass resonance Q value.  0.0 to 127.0.  Default = 1.0. */
    }

    public enum DSP_HIGHPASS : int
    {
        CUTOFF,    /* (Type:float) - Highpass cutoff frequency in hz.  1.0 to output 22000.0.  Default = 5000.0. */
        RESONANCE  /* (Type:float) - Highpass resonance Q value.  1.0 to 10.0.  Default = 1.0. */
    }

    public enum DSP_ECHO : int
    {
        DELAY,       /* (Type:float) - Echo delay in ms.  10  to 5000.  Default = 500. */
        FEEDBACK,    /* (Type:float) - Echo decay per delay.  0 to 100.  100.0 = No decay, 0.0 = total decay (ie simple 1 line delay).  Default = 50.0. */
        DRYLEVEL,    /* (Type:float) - Original sound volume in dB.  -80.0 to 10.0.  Default = 0. */
        WETLEVEL     /* (Type:float) - Volume of echo signal to pass to output in dB.  -80.0 to 10.0.  Default = 0. */
    }

    public enum DSP_FADER : int
    {
        GAIN,           /* (Type:float) - Signal gain in dB. -80.0 to 10.0. Default = 0.0. */
        OVERALL_GAIN,   /* (Type:data)  - Overall gain. For information only, not set by user. Data of type FMOD_DSP_PARAMETER_DATA_TYPE_OVERALLGAIN to provide to FMOD, to allow FMOD to know the DSP is scaling the signal for virtualization purposes. */
    }

    public enum DSP_DELAY : int
    {
        CH0,      /* Channel #0 Delay in ms.   0  to 10000.  Default = 0.  */
        CH1,      /* Channel #1 Delay in ms.   0  to 10000.  Default = 0.  */
        CH2,      /* Channel #2 Delay in ms.   0  to 10000.  Default = 0.  */
        CH3,      /* Channel #3 Delay in ms.   0  to 10000.  Default = 0.  */
        CH4,      /* Channel #4 Delay in ms.   0  to 10000.  Default = 0.  */
        CH5,      /* Channel #5 Delay in ms.   0  to 10000.  Default = 0.  */
        CH6,      /* Channel #6 Delay in ms.   0  to 10000.  Default = 0.  */
        CH7,      /* Channel #7 Delay in ms.   0  to 10000.  Default = 0.  */
        CH8,      /* Channel #8 Delay in ms.   0  to 10000.  Default = 0.  */
        CH9,      /* Channel #9 Delay in ms.   0  to 10000.  Default = 0.  */
        CH10,     /* Channel #10 Delay in ms.  0  to 10000.  Default = 0.  */
        CH11,     /* Channel #11 Delay in ms.  0  to 10000.  Default = 0.  */
        CH12,     /* Channel #12 Delay in ms.  0  to 10000.  Default = 0.  */
        CH13,     /* Channel #13 Delay in ms.  0  to 10000.  Default = 0.  */
        CH14,     /* Channel #14 Delay in ms.  0  to 10000.  Default = 0.  */
        CH15,     /* Channel #15 Delay in ms.  0  to 10000.  Default = 0.  */
        MAXDELAY, /* Maximum delay in ms.      0  to 1000.   Default = 10. */
    }

    public enum DSP_FLANGE : int
    {
        MIX,         /* (Type:float) - Percentage of wet signal in mix.  0 to 100. Default = 50. */
        DEPTH,       /* (Type:float) - Flange depth (percentage of 40ms delay).  0.01 to 1.0.  Default = 1.0. */
        RATE         /* (Type:float) - Flange speed in hz.  0.0 to 20.0.  Default = 0.1. */
    }

    public enum DSP_TREMOLO : int
    {
        FREQUENCY,     /* LFO frequency in Hz.  0.1 to 20.  Default = 4. */
        DEPTH,         /* Tremolo depth.  0 to 1.  Default = 0. */
        SHAPE,         /* LFO shape morph between triangle and sine.  0 to 1.  Default = 0. */
        SKEW,          /* Time-skewing of LFO cycle.  -1 to 1.  Default = 0. */
        DUTY,          /* LFO on-time.  0 to 1.  Default = 0.5. */
        SQUARE,        /* Flatness of the LFO shape.  0 to 1.  Default = 0. */
        PHASE,         /* Instantaneous LFO phase.  0 to 1.  Default = 0. */
        SPREAD         /* Rotation / auto-pan effect.  -1 to 1.  Default = 0. */
    }

    public enum DSP_DISTORTION : int
    {
        LEVEL    /* Distortion value.  0.0 to 1.0.  Default = 0.5. */
    }

    public enum DSP_NORMALIZE : int
    {
        FADETIME,    /* Time to ramp the silence to full in ms.  0.0 to 20000.0. Default = 5000.0. */
        THRESHHOLD,  /* Lower volume range threshold to ignore.  0.0 to 1.0.  Default = 0.1.  Raise higher to stop amplification of very quiet signals. */
        MAXAMP       /* Maximum amplification allowed.  1.0 to 100000.0.  Default = 20.0.  1.0 = no amplifaction, higher values allow more boost. */
    }

    public enum DSP_LIMITER : int
    {
        RELEASETIME,   /* (Type:float) - Time to ramp the silence to full in ms.  1.0 to 1000.0. Default = 10.0. */
        CEILING,       /* (Type:float) - Maximum level of the output signal in dB.  -12.0 to 0.0.  Default = 0.0. */
        MAXIMIZERGAIN, /* (Type:float) - Maximum amplification allowed in dB.  0.0 to 12.0.  Default = 0.0. 0.0 = no amplifaction, higher values allow more boost. */
        MODE,          /* (Type:float) - Channel processing mode. 0 or 1. Default = 0. 0 = Independent (limiter per channel), 1 = Linked. */
    }
    
    public enum DSP_PARAMEQ : int
    {
        CENTER,     /* Frequency center.  20.0 to 22000.0.  Default = 8000.0. */
        BANDWIDTH,  /* Octave range around the center frequency to filter.  0.2 to 5.0.  Default = 1.0. */
        GAIN        /* Frequency Gain.  0.05 to 3.0.  Default = 1.0.  */
    }

    public enum DSP_MULTIBAND_EQ : int
    {
        A_FILTER,    /* (Type:int)   - Band A: FMOD_DSP_MULTIBAND_EQ_FILTER_TYPE used to interpret the behavior of the remaining parameters. Default = FMOD_DSP_MULTIBAND_EQ_FILTER_LOWPASS_12DB */
        A_FREQUENCY, /* (Type:float) - Band A: Significant frequency in Hz, cutoff [low/high pass, low/high shelf], center [notch, peaking, band-pass], phase transition point [all-pass]. 20 to 24000. Default = 8000. */
        A_Q,         /* (Type:float) - Band A: Quality factory, resonance [low/high pass], bandwidth [notch, peaking, band-pass], phase transition sharpness [all-pass], unused [low/high shelf]. 0.1 to 10.0. Default = 0.707. */
        A_GAIN,      /* (Type:float) - Band A: Boost or attenuation in dB [peaking, high/low shelf only]. -30 to 30. Default = 0. */
        B_FILTER,    /* (Type:int)   - Band B: See Band A. Default = FMOD_DSP_MULTIBAND_EQ_FILTER_DISABLED */
        B_FREQUENCY, /* (Type:float) - Band B: See Band A */
        B_Q,         /* (Type:float) - Band B: See Band A */
        B_GAIN,      /* (Type:float) - Band B: See Band A */
        C_FILTER,    /* (Type:int)   - Band C: See Band A. Default = FMOD_DSP_MULTIBAND_EQ_FILTER_DISABLED */
        C_FREQUENCY, /* (Type:float) - Band C: See Band A. */
        C_Q,         /* (Type:float) - Band C: See Band A. */
        C_GAIN,      /* (Type:float) - Band C: See Band A. */
        D_FILTER,    /* (Type:int)   - Band D: See Band A. Default = FMOD_DSP_MULTIBAND_EQ_FILTER_DISABLED */
        D_FREQUENCY, /* (Type:float) - Band D: See Band A. */
        D_Q,         /* (Type:float) - Band D: See Band A. */
        D_GAIN,      /* (Type:float) - Band D: See Band A. */
        E_FILTER,    /* (Type:int)   - Band E: See Band A. Default = FMOD_DSP_MULTIBAND_EQ_FILTER_DISABLED */
        E_FREQUENCY, /* (Type:float) - Band E: See Band A. */
        E_Q,         /* (Type:float) - Band E: See Band A. */
        E_GAIN,      /* (Type:float) - Band E: See Band A. */
    }

    public enum DSP_MULTIBAND_EQ_FILTER_TYPE : int
    {
        DISABLED,       /* Disabled filter, no processing. */
        LOWPASS_12DB,   /* Resonant low-pass filter, attenuates frequencies (12dB per octave) above a given point (with specificed resonance) while allowing the rest to pass. */
        LOWPASS_24DB,   /* Resonant low-pass filter, attenuates frequencies (24dB per octave) above a given point (with specificed resonance) while allowing the rest to pass. */
        LOWPASS_48DB,   /* Resonant low-pass filter, attenuates frequencies (48dB per octave) above a given point (with specificed resonance) while allowing the rest to pass. */
        HIGHPASS_12DB,  /* Resonant low-pass filter, attenuates frequencies (12dB per octave) below a given point (with specificed resonance) while allowing the rest to pass. */
        HIGHPASS_24DB,  /* Resonant low-pass filter, attenuates frequencies (24dB per octave) below a given point (with specificed resonance) while allowing the rest to pass. */
        HIGHPASS_48DB,  /* Resonant low-pass filter, attenuates frequencies (48dB per octave) below a given point (with specificed resonance) while allowing the rest to pass. */
        LOWSHELF,       /* Low-shelf filter, boosts or attenuates frequencies (with specified gain) below a given point while allowing the rest to pass. */
        HIGHSHELF,      /* High-shelf filter, boosts or attenuates frequencies (with specified gain) above a given point while allowing the rest to pass. */
        PEAKING,        /* Peaking filter, boosts or attenuates frequencies (with specified gain) at a given point (with specificed bandwidth) while allowing the rest to pass. */
        BANDPASS,       /* Band-pass filter, allows frequencies at a given point (with specificed bandwidth) to pass while attenuating frequencies outside this range. */
        NOTCH,          /* Notch or band-reject filter, attenuates frequencies at a given point (with specificed bandwidth) while allowing frequencies outside this range to pass. */
        ALLPASS,        /* All-pass filter, allows all frequencies to pass, but changes the phase response at a given point (with specified sharpness). */
    }

    public enum DSP_PITCHSHIFT : int
    {
        PITCH,       /* Pitch value.  0.5 to 2.0.  Default = 1.0. 0.5 = one octave down, 2.0 = one octave up.  1.0 does not change the pitch. */
        FFTSIZE,     /* FFT window size.  256, 512, 1024, 2048, 4096.  Default = 1024.  Increase this to reduce 'smearing'.  This effect is a warbling sound similar to when an mp3 is encoded at very low bitrates. */
        OVERLAP,     /* Window overlap.  1 to 32.  Default = 4.  Increase this to reduce 'tremolo' effect.  Increasing it by a factor of 2 doubles the CPU usage. */
        MAXCHANNELS  /* Maximum channels supported.  0 to 16.  0 = same as fmod's default output polyphony, 1 = mono, 2 = stereo etc.  See remarks for more.  Default = 0.  It is suggested to leave at 0! */
    }

    public enum DSP_CHORUS : int
    {
        MIX,      /* (Type:float) - Volume of original signal to pass to output.  0.0 to 100.0. Default = 50.0. */
        RATE,     /* (Type:float) - Chorus modulation rate in Hz.  0.0 to 20.0.  Default = 0.8 Hz. */
        DEPTH,    /* (Type:float) - Chorus modulation depth.  0.0 to 100.0.  Default = 3.0. */
    }

    public enum DSP_ITECHO : int
    {
        WETDRYMIX,      /* (Type:float) - Ratio of wet (processed) signal to dry (unprocessed) signal. Must be in the range from 0.0 through 100.0 (all wet).  Default = 50. */
        FEEDBACK,       /* (Type:float) - Percentage of output fed back into input, in the range from 0.0 through 100.0.  Default = 50. */
        LEFTDELAY,      /* (Type:float) - Delay for left channel, in milliseconds, in the range from 1.0 through 2000.0.  Default = 500 ms. */
        RIGHTDELAY,     /* (Type:float) - Delay for right channel, in milliseconds, in the range from 1.0 through 2000.0.  Default = 500 ms. */
        PANDELAY        /* (Type:float) - Value that specifies whether to swap left and right delays with each successive echo.  Ranges from 0.0 (equivalent to FALSE) to 1.0 (equivalent to TRUE), meaning no swap.  Default = 0.  CURRENTLY NOT SUPPORTED. */
    }

    public enum DSP_COMPRESSOR : int
    {
        THRESHOLD,   /* (Type:float) - Threshold level (dB) in the range from -80 through 0. The default value is 0. */ 
        RATIO,       /* (Type:float) - Compression Ratio (dB/dB) in the range from 1 to 50. The default value is 2.5. */ 
        ATTACK,      /* (Type:float) - Attack time (milliseconds), in the range from 0.1 through 1000. The default value is 20. */
        RELEASE,     /* (Type:float) - Release time (milliseconds), in the range from 10 through 5000. The default value is 100 */
        GAINMAKEUP,  /* (Type:float) - Make-up gain (dB) applied after limiting, in the range from 0 through 30. The default value is 0. */
        USESIDECHAIN,/* (Type:bool)  - Whether to analyse the sidechain signal instead of the input signal. The default value is false */
        LINKED       /* (Type:bool)  - FALSE = Independent (compressor per channel), TRUE = Linked.  The default value is TRUE. */
    }

    public enum DSP_SFXREVERB : int
    {
        DECAYTIME,           /* (Type:float) - Decay Time       : Reverberation decay time at low-frequencies in milliseconds.  Ranges from 100.0 to 20000.0. Default is 1500. */
        EARLYDELAY,          /* (Type:float) - Early Delay      : Delay time of first reflection in milliseconds.  Ranges from 0.0 to 300.0.  Default is 20. */
        LATEDELAY,           /* (Type:float) - Reverb Delay     : Late reverberation delay time relative to first reflection in milliseconds.  Ranges from 0.0 to 100.0.  Default is 40. */
        HFREFERENCE,         /* (Type:float) - HF Reference     : Reference frequency for high-frequency decay in Hz.  Ranges from 20.0 to 20000.0. Default is 5000. */
        HFDECAYRATIO,        /* (Type:float) - Decay HF Ratio   : High-frequency decay time relative to decay time in percent.  Ranges from 10.0 to 100.0. Default is 50. */
        DIFFUSION,           /* (Type:float) - Diffusion        : Reverberation diffusion (echo density) in percent.  Ranges from 0.0 to 100.0.  Default is 100. */
        DENSITY,             /* (Type:float) - Density          : Reverberation density (modal density) in percent.  Ranges from 0.0 to 100.0.  Default is 100. */
        LOWSHELFFREQUENCY,   /* (Type:float) - Low Shelf Frequency : Transition frequency of low-shelf filter in Hz.  Ranges from 20.0 to 1000.0. Default is 250. */
        LOWSHELFGAIN,        /* (Type:float) - Low Shelf Gain   : Gain of low-shelf filter in dB.  Ranges from -36.0 to 12.0.  Default is 0. */
        HIGHCUT,             /* (Type:float) - High Cut         : Cutoff frequency of low-pass filter in Hz.  Ranges from 20.0 to 20000.0. Default is 20000. */
        EARLYLATEMIX,        /* (Type:float) - Early/Late Mix   : Blend ratio of late reverb to early reflections in percent.  Ranges from 0.0 to 100.0.  Default is 50. */
        WETLEVEL,            /* (Type:float) - Wet Level        : Reverb signal level in dB.  Ranges from -80.0 to 20.0.  Default is -6. */
        DRYLEVEL             /* (Type:float) - Dry Level        : Dry signal level in dB.  Ranges from -80.0 to 20.0.  Default is 0. */
    }

    public enum DSP_LOWPASS_SIMPLE : int
    {
        CUTOFF     /* Lowpass cutoff frequency in hz.  10.0 to 22000.0.  Default = 5000.0 */
    }

    public enum DSP_SEND : int
    {
        RETURNID,     /* (Type:int) - ID of the Return DSP this send is connected to (integer values only). -1 indicates no connected Return DSP. Default = -1. */
        LEVEL,        /* (Type:float) - Send level. 0.0 to 1.0. Default = 1.0 */
    }

    public enum DSP_RETURN : int
    {
        ID,                 /* (Type:int) - ID of this Return DSP. Read-only.  Default = -1. */
        INPUT_SPEAKER_MODE  /* (Type:int) - Input speaker mode of this return.  Default = FMOD_SPEAKERMODE_DEFAULT. */
    }

    public enum DSP_HIGHPASS_SIMPLE : int
    {
        CUTOFF     /* (Type:float) - Highpass cutoff frequency in hz.  10.0 to 22000.0.  Default = 1000.0 */
    }

    public enum DSP_PAN_2D_STEREO_MODE_TYPE : int
    {
        DISTRIBUTED,        /* The parts of a stereo sound are spread around desination speakers based on FMOD_DSP_PAN_2D_EXTENT / FMOD_DSP_PAN_2D_DIRECTION */
        DISCRETE            /* The L/R parts of a stereo sound are rotated around a circle based on FMOD_DSP_PAN_2D_STEREO_AXIS / FMOD_DSP_PAN_2D_STEREO_SEPARATION. */
    }

    public enum DSP_PAN_MODE_TYPE : int
    {
        MONO,
        STEREO,
        SURROUND
    }

    public enum DSP_PAN_3D_ROLLOFF_TYPE : int
    {
        LINEARSQUARED,
        LINEAR,
        INVERSE,
        INVERSETAPERED,
        CUSTOM
    }

    public enum DSP_PAN_3D_EXTENT_MODE_TYPE : int
    {
        AUTO,
        USER,
        OFF
    }

    public enum DSP_PAN : int
    {
        MODE,                          /* (Type:int)   - Panner mode.               FMOD_DSP_PAN_MODE_MONO for mono down-mix, FMOD_DSP_PAN_MODE_STEREO for stereo panning or FMOD_DSP_PAN_MODE_SURROUND for surround panning.  Default = FMOD_DSP_PAN_MODE_SURROUND */
        _2D_STEREO_POSITION,           /* (Type:float) - 2D Stereo pan position.    -100.0 to 100.0.  Default = 0.0. */
        _2D_DIRECTION,                 /* (Type:float) - 2D Surround pan direction. Direction from center point of panning circle. -180.0 (degrees) to 180.0 (degrees).  0 = front center, -180 or +180 = rear speakers center point. Default = 0.0. */
        _2D_EXTENT,                    /* (Type:float) - 2D Surround pan extent.    Distance from center point of panning circle.  0.0 (degrees) to 360.0 (degrees).  Default = 360.0. */
        _2D_ROTATION,                  /* (Type:float) - 2D Surround pan rotation.  -180.0 (degrees) to 180.0 (degrees).  Default = 0.0. */
        _2D_LFE_LEVEL,                 /* (Type:float) - 2D Surround pan LFE level. 2D LFE level in dB.  -80.0 (db) to 20.0 (db).  Default = 0.0. */
        _2D_STEREO_MODE,               /* (Type:int)   - Stereo-To-Surround Mode.   FMOD_DSP_PAN_2D_STEREO_MODE_DISTRIBUTED to FMOD_DSP_PAN_2D_STEREO_MODE_DISCRETE.  Default = FMOD_DSP_PAN_2D_STEREO_MODE_DISCRETE.*/
        _2D_STEREO_SEPARATION,         /* (Type:float) - Stereo-To-Surround Stereo  For FMOD_DSP_PAN_2D_STEREO_MODE_DISCRETE mode.  Separation/width of L/R parts of stereo sound. -180.0 (degrees) to +180.0 (degrees).  Default = 60.0. */
        _2D_STEREO_AXIS,               /* (Type:float) - Stereo-To-Surround Stereo  For FMOD_DSP_PAN_2D_STEREO_MODE_DISCRETE mode.  Axis/rotation of L/R parts of stereo sound. -180.0 (degrees) to +180.0 (degrees).  Default = 0.0. */
        ENABLED_SPEAKERS,              /* (Type:int)   - Speakers Enabled.          Bitmask for each speaker from 0 to 32 to be considered by panner.  Use to disable speakers from being panned to.  0 to 0xFFF.  Default = 0xFFF (All on).  */
        _3D_POSITION,                  /* (Type:data)  - 3D Position.               Data of type FMOD_DSP_PARAMETER_3DATTRIBUTES_MULTI.  See remarks on what to fill out. */
        _3D_ROLLOFF,                   /* (Type:int)   - 3D Rolloff.                FMOD_DSP_PAN_3D_ROLLOFF_LINEARSQUARED to FMOD_DSP_PAN_3D_ROLLOFF_CUSTOM.  Default = FMOD_DSP_PAN_3D_ROLLOFF_LINEARSQUARED. */
        _3D_MIN_DISTANCE,              /* (Type:float) - 3D Min Distance.           0.0 to 1e+18f.  Default = 1.0. */
        _3D_MAX_DISTANCE,              /* (Type:float) - 3D Max Distance.           0.0 to 1e+18f.  Default = 20.0. */
        _3D_EXTENT_MODE,               /* (Type:int)   - 3D Extent Mode.            FMOD_DSP_PAN_3D_EXTENT_MODE_AUTO to FMOD_DSP_PAN_3D_EXTENT_MODE_OFF.  Default = FMOD_DSP_PAN_3D_EXTENT_MODE_AUTO. */
        _3D_SOUND_SIZE,                /* (Type:float) - 3D Sound Size.             0.0 to 1e+18f.  Default = 0.0. */
        _3D_MIN_EXTENT,                /* (Type:float) - 3D Min Extent.             0.0 (degrees) to 360.0 (degrees).  Default = 0.0. */
        _3D_PAN_BLEND,                 /* (Type:float) - 3D Pan Blend.              0.0 (fully 2D) to 1.0 (fully 3D).  Default = 0.0. */
        LFE_UPMIX_ENABLED,             /* (Type:int)   - LFE Upmix Enabled.         Determines whether non-LFE source channels should mix to the LFE or leave it alone.  0 (off) to 1 (on).  Default = 0 (off). */
        OVERALL_GAIN,                  /* (Type:data)  - Overall gain.              For information only, not set by user.  Data of type FMOD_DSP_PARAMETER_DATA_TYPE_OVERALLGAIN to provide to FMOD, to allow FMOD to know the DSP is scaling the signal for virtualization purposes. */
        SURROUND_SPEAKER_MODE,         /* (Type:int)   - Surround speaker mode.     Target speaker mode for surround panning.  Default = FMOD_SPEAKERMODE_DEFAULT. */
        _2D_HEIGHT_BLEND,              /* (Type:float) - 2D Height blend.           When FMOD_DSP_PAN_SURROUND_SPEAKER_MODE has height speakers, control the blend between ground and height. 0.0 (ground speakers only) to 1.0 (top speakers only). Default = 0.0. */
    }

    public enum DSP_THREE_EQ_CROSSOVERSLOPE_TYPE : int
    {
        _12DB,
        _24DB,
        _48DB
    }

    public enum DSP_THREE_EQ : int
    {
        LOWGAIN,       /* (Type:float) - Low frequency gain in dB.  -80.0 to 10.0.  Default = 0. */
        MIDGAIN,       /* (Type:float) - Mid frequency gain in dB.  -80.0 to 10.0.  Default = 0. */
        HIGHGAIN,      /* (Type:float) - High frequency gain in dB.  -80.0 to 10.0.  Default = 0. */
        LOWCROSSOVER,  /* (Type:float) - Low-to-mid crossover frequency in Hz.  10.0 to 22000.0.  Default = 400.0. */
        HIGHCROSSOVER, /* (Type:float) - Mid-to-high crossover frequency in Hz.  10.0 to 22000.0.  Default = 4000.0. */
        CROSSOVERSLOPE /* (Type:int)   - Crossover Slope.  0 = 12dB/Octave, 1 = 24dB/Octave, 2 = 48dB/Octave.  Default = 1 (24dB/Octave). */
    }

    public enum DSP_FFT_WINDOW : int
    {
        RECT,            /* w[n] = 1.0                                                                                            */
        TRIANGLE,        /* w[n] = TRI(2n/N)                                                                                      */
        HAMMING,         /* w[n] = 0.54 - (0.46 * COS(n/N) )                                                                      */
        HANNING,         /* w[n] = 0.5 *  (1.0  - COS(n/N) )                                                                      */
        BLACKMAN,        /* w[n] = 0.42 - (0.5  * COS(n/N) ) + (0.08 * COS(2.0 * n/N) )                                           */
        BLACKMANHARRIS   /* w[n] = 0.35875 - (0.48829 * COS(1.0 * n/N)) + (0.14128 * COS(2.0 * n/N)) - (0.01168 * COS(3.0 * n/N)) */
    }

    public enum DSP_FFT : int
    {
        WINDOWSIZE,            /*  (Type:int)   - [r/w] Must be a power of 2 between 128 and 16384.  128, 256, 512, 1024, 2048, 4096, 8192, 16384 are accepted.  Default = 2048. */
        WINDOWTYPE,            /*  (Type:int)   - [r/w] Refer to FMOD_DSP_FFT_WINDOW enumeration.  Default = FMOD_DSP_FFT_WINDOW_HAMMING. */
        SPECTRUMDATA,          /*  (Type:data)  - [r]   Returns the current spectrum values between 0 and 1 for each 'fft bin'.  Cast data to FMOD_DSP_PARAMETER_DATA_TYPE_FFT.  Divide the niquist rate by the window size to get the hz value per entry. */
        DOMINANT_FREQ          /*  (Type:float) - [r]   Returns the dominant frequencies for each channel. */
    }

    public enum DSP_ENVELOPEFOLLOWER : int
    {
        ATTACK,      /* (Type:float) - Attack time (milliseconds), in the range from 0.1 through 1000. The default value is 20. */
        RELEASE,     /* (Type:float) - Release time (milliseconds), in the range from 10 through 5000. The default value is 100 */
        ENVELOPE,    /* (Type:float) - Current value of the envelope, in the range 0 to 1. Read-only. */
        USESIDECHAIN /* (Type:bool)  - Whether to analyse the sidechain signal instead of the input signal. The default value is false */
    }

    public enum DSP_CONVOLUTION_REVERB : int
    {
        IR,       /* (Type:data)  - [w]   16-bit reverb IR (short*) with an extra sample prepended to the start which specifies the number of channels. */
        WET,      /* (Type:float) - [r/w] Volume of echo signal to pass to output in dB.  -80.0 to 10.0.  Default = 0. */
        DRY,      /* (Type:float) - [r/w] Original sound volume in dB.  -80.0 to 10.0.  Default = 0. */
        LINKED    /* (Type:bool)  - [r/w] Linked - channels are mixed together before processing through the reverb.  Default = TRUE. */
    }

    public enum DSP_CHANNELMIX_OUTPUT : int
    {
        DEFAULT,            /*  Output channel count = input channel count.  Mapping: See FMOD_SPEAKER enumeration. */
        ALLMONO,            /*  Output channel count = 1.  Mapping: Mono, Mono, Mono, Mono, Mono, Mono, ... (each channel all the way up to FMOD_MAX_CHANNEL_WIDTH channels are treated as if they were mono) */
        ALLSTEREO,          /*  Output channel count = 2.  Mapping: Left, Right, Left, Right, Left, Right, ... (each pair of channels is treated as stereo all the way up to FMOD_MAX_CHANNEL_WIDTH channels) */
        ALLQUAD,            /*  Output channel count = 4.  Mapping: Repeating pattern of Front Left, Front Right, Surround Left, Surround Right. */
        ALL5POINT1,         /*  Output channel count = 6.  Mapping: Repeating pattern of Front Left, Front Right, Center, LFE, Surround Left, Surround Right. */
        ALL7POINT1,         /*  Output channel count = 8.  Mapping: Repeating pattern of Front Left, Front Right, Center, LFE, Surround Left, Surround Right, Back Left, Back Right.  */
        ALLLFE,             /*  Output channel count = 6.  Mapping: Repeating pattern of LFE in a 5.1 output signal.  */
        ALL7POINT1POINT4    /*  Output channel count = 12. Mapping: Repeating pattern of Front Left, Front Right, Center, LFE, Surround Left, Surround Right, Back Left, Back Right, Top Front Left, Top Front Right, Top Back Left, Top Back Right.  */
    }

    public enum DSP_CHANNELMIX : int
    {
        OUTPUTGROUPING,     /* (Type:int)   - Refer to FMOD_DSP_CHANNELMIX_OUTPUT enumeration.  Default = FMOD_DSP_CHANNELMIX_OUTPUT_DEFAULT.  See remarks. */
        GAIN_CH0,           /* (Type:float) - Channel  #0 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH1,           /* (Type:float) - Channel  #1 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH2,           /* (Type:float) - Channel  #2 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH3,           /* (Type:float) - Channel  #3 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH4,           /* (Type:float) - Channel  #4 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH5,           /* (Type:float) - Channel  #5 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH6,           /* (Type:float) - Channel  #6 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH7,           /* (Type:float) - Channel  #7 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH8,           /* (Type:float) - Channel  #8 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH9,           /* (Type:float) - Channel  #9 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH10,          /* (Type:float) - Channel #10 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH11,          /* (Type:float) - Channel #11 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH12,          /* (Type:float) - Channel #12 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH13,          /* (Type:float) - Channel #13 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH14,          /* (Type:float) - Channel #14 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH15,          /* (Type:float) - Channel #15 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH16,          /* (Type:float) - Channel #16 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH17,          /* (Type:float) - Channel #17 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH18,          /* (Type:float) - Channel #18 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH19,          /* (Type:float) - Channel #19 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH20,          /* (Type:float) - Channel #20 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH21,          /* (Type:float) - Channel #21 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH22,          /* (Type:float) - Channel #22 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH23,          /* (Type:float) - Channel #23 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH24,          /* (Type:float) - Channel #24 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH25,          /* (Type:float) - Channel #25 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH26,          /* (Type:float) - Channel #26 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH27,          /* (Type:float) - Channel #27 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH28,          /* (Type:float) - Channel #28 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH29,          /* (Type:float) - Channel #29 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH30,          /* (Type:float) - Channel #30 gain in dB.  -80.0 to 10.0.  Default = 0. */
        GAIN_CH31,          /* (Type:float) - Channel #31 gain in dB.  -80.0 to 10.0.  Default = 0. */
        OUTPUT_CH0,         /* (Type:int)   - Input channel #0  Output channel.   0 to 31.  Default = 0. */
        OUTPUT_CH1,         /* (Type:int)   - Input channel #1  Output channel.   0 to 31.  Default = 1. */
        OUTPUT_CH2,         /* (Type:int)   - Input channel #2  Output channel.   0 to 31.  Default = 2. */
        OUTPUT_CH3,         /* (Type:int)   - Input channel #3  Output channel.   0 to 31.  Default = 3. */
        OUTPUT_CH4,         /* (Type:int)   - Input channel #4  Output channel.   0 to 31.  Default = 4. */
        OUTPUT_CH5,         /* (Type:int)   - Input channel #5  Output channel.   0 to 31.  Default = 5. */
        OUTPUT_CH6,         /* (Type:int)   - Input channel #6  Output channel.   0 to 31.  Default = 6. */
        OUTPUT_CH7,         /* (Type:int)   - Input channel #7  Output channel.   0 to 31.  Default = 7. */
        OUTPUT_CH8,         /* (Type:int)   - Input channel #8  Output channel.   0 to 31.  Default = 8. */
        OUTPUT_CH9,         /* (Type:int)   - Input channel #9  Output channel.   0 to 31.  Default = 9. */
        OUTPUT_CH10,        /* (Type:int)   - Input channel #10 Output channel.   0 to 31.  Default = 10. */
        OUTPUT_CH11,        /* (Type:int)   - Input channel #11 Output channel.   0 to 31.  Default = 11. */
        OUTPUT_CH12,        /* (Type:int)   - Input channel #12 Output channel.   0 to 31.  Default = 12. */
        OUTPUT_CH13,        /* (Type:int)   - Input channel #13 Output channel.   0 to 31.  Default = 13. */
        OUTPUT_CH14,        /* (Type:int)   - Input channel #14 Output channel.   0 to 31.  Default = 14. */
        OUTPUT_CH15,        /* (Type:int)   - Input channel #15 Output channel.   0 to 31.  Default = 15. */
        OUTPUT_CH16,        /* (Type:int)   - Input channel #16 Output channel.   0 to 31.  Default = 16. */
        OUTPUT_CH17,        /* (Type:int)   - Input channel #17 Output channel.   0 to 31.  Default = 17. */
        OUTPUT_CH18,        /* (Type:int)   - Input channel #18 Output channel.   0 to 31.  Default = 18. */
        OUTPUT_CH19,        /* (Type:int)   - Input channel #19 Output channel.   0 to 31.  Default = 19. */
        OUTPUT_CH20,        /* (Type:int)   - Input channel #20 Output channel.   0 to 31.  Default = 20. */
        OUTPUT_CH21,        /* (Type:int)   - Input channel #21 Output channel.   0 to 31.  Default = 21. */
        OUTPUT_CH22,        /* (Type:int)   - Input channel #22 Output channel.   0 to 31.  Default = 22. */
        OUTPUT_CH23,        /* (Type:int)   - Input channel #23 Output channel.   0 to 31.  Default = 23. */
        OUTPUT_CH24,        /* (Type:int)   - Input channel #24 Output channel.   0 to 31.  Default = 24. */
        OUTPUT_CH25,        /* (Type:int)   - Input channel #25 Output channel.   0 to 31.  Default = 25. */
        OUTPUT_CH26,        /* (Type:int)   - Input channel #26 Output channel.   0 to 31.  Default = 26. */
        OUTPUT_CH27,        /* (Type:int)   - Input channel #27 Output channel.   0 to 31.  Default = 27. */
        OUTPUT_CH28,        /* (Type:int)   - Input channel #28 Output channel.   0 to 31.  Default = 28. */
        OUTPUT_CH29,        /* (Type:int)   - Input channel #29 Output channel.   0 to 31.  Default = 29. */
        OUTPUT_CH30,        /* (Type:int)   - Input channel #30 Output channel.   0 to 31.  Default = 30. */
        OUTPUT_CH31,        /* (Type:int)   - Input channel #31 Output channel.   0 to 31.  Default = 31. */
    }

    public enum DSP_TRANSCEIVER_SPEAKERMODE : int
    {
        AUTO = -1,     /* A transmitter will use whatever signal channel count coming in to the transmitter, to determine which speaker mode is allocated for the transceiver channel. */
        MONO = 0,      /* A transmitter will always downmix to a mono channel buffer. */
        STEREO,        /* A transmitter will always upmix or downmix to a stereo channel buffer. */
        SURROUND,      /* A transmitter will always upmix or downmix to a surround channel buffer.   Surround is the speaker mode of the system above stereo, so could be quad/surround/5.1/7.1. */
    }

    public enum DSP_TRANSCEIVER : int
    {
        TRANSMIT,            /* (Type:bool)  - [r/w] - FALSE = Transceiver is a 'receiver' (like a return) and accepts data from a channel.  TRUE = Transceiver is a 'transmitter' (like a send).  Default = FALSE. */
        GAIN,                /* (Type:float) - [r/w] - Gain to receive or transmit at in dB.  -80.0 to 10.0.  Default = 0. */
        CHANNEL,             /* (Type:int)   - [r/w] - Integer to select current global slot, shared by all Transceivers, that can be transmitted to or received from.  0 to 31.  Default = 0.*/
        TRANSMITSPEAKERMODE  /* (Type:int)   - [r/w] - Speaker mode (transmitter mode only).  Specifies either 0 (Auto) Default = 0.*/
    }

    public enum DSP_OBJECTPAN : int
    {
        _3D_POSITION,        /* (Type:data)  - 3D Position.               data of type FMOD_DSP_PARAMETER_3DATTRIBUTES_MULTI */
        _3D_ROLLOFF,         /* (Type:int)   - 3D Rolloff.                FMOD_DSP_PAN_3D_ROLLOFF_LINEARSQUARED to FMOD_DSP_PAN_3D_ROLLOFF_CUSTOM.  Default = FMOD_DSP_PAN_3D_ROLLOFF_LINEARSQUARED. */
        _3D_MIN_DISTANCE,    /* (Type:float) - 3D Min Distance.           0.0 to 1e+19f.  Default = 1.0. */
        _3D_MAX_DISTANCE,    /* (Type:float) - 3D Max Distance.           0.0 to 1e+19f.  Default = 20.0. */
        _3D_EXTENT_MODE,     /* (Type:int)   - 3D Extent Mode.            FMOD_DSP_PAN_3D_EXTENT_MODE_AUTO to FMOD_DSP_PAN_3D_EXTENT_MODE_OFF.  Default = FMOD_DSP_PAN_3D_EXTENT_MODE_AUTO. */
        _3D_SOUND_SIZE,      /* (Type:float) - 3D Sound Size.             0.0 to 1e+19f.  Default = 0.0. */
        _3D_MIN_EXTENT,      /* (Type:float) - 3D Min Extent.             0.0 (degrees) to 360.0 (degrees).  Default = 0.0. */
        OVERALL_GAIN,        /* (Type:data)  - Overall gain.              For information only, not set by user.  Data of type FMOD_DSP_PARAMETER_DATA_TYPE_OVERALLGAIN to provide to FMOD, to allow FMOD to know the DSP is scaling the signal for virtualization purposes. */
        OUTPUTGAIN           /* (Type:float) - Output gain level.         0.0 to 1.0 linear scale.  For the user to scale the output of the object panner's signal. */
    }
}
