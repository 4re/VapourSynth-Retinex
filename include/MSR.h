/*
* Retinex filter - VapourSynth plugin
* Copyright (C) 2014  mawen1250
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef MSR_H_
#define MSR_H_


#include "Helper.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const struct MSRPara
{
    std::vector<double> sigma;

    double lower_thr = 0.001;
    double upper_thr = 0.001;
    int HistBins = 4096;

    bool fulls = true;
    bool fulld = fulls;

    MSRPara() : sigma({ 25, 80, 250 }) {}
} MSRDefault;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class MSRData
    : public VSData
{
public:
    std::vector<double> sigma = MSRDefault.sigma;

    double lower_thr = MSRDefault.lower_thr;
    double upper_thr = MSRDefault.upper_thr;
    int HistBins = MSRDefault.HistBins;

    bool fulls = MSRDefault.fulls;
    bool fulld = MSRDefault.fulld;

private:
    void fulls_select()
    {
        if (vi->format->colorFamily == cmGray || vi->format->colorFamily == cmYUV)
            fulls = false;
        else if (vi->format->colorFamily == cmRGB || vi->format->colorFamily == cmYCoCg)
            fulls = true;
    }

public:
    MSRData(const VSAPI *_vsapi = nullptr, std::string _FunctionName = "MSR", std::string _NameSpace = "retinex")
        : VSData(_vsapi, _FunctionName, _NameSpace) {}

    virtual ~MSRData() {}

    virtual int arguments_process(const VSMap *in, VSMap *out)
    {
        int error;
        int i, m;

        node = vsapi->propGetNode(in, "input", 0, nullptr);
        vi = vsapi->getVideoInfo(node);

        if (!vi->format)
        {
            setError(out, "Invalid input clip, only constant format input supported");
            return 1;
        }
        if (vi->format->sampleType != stInteger || (vi->format->bytesPerSample != 1 && vi->format->bytesPerSample != 2))
        {
            setError(out, "Invalid input clip, only 8-16 bit int formats supported");
            return 1;
        }
        if (vi->format->subSamplingH || vi->format->subSamplingW)
        {
            setError(out, "sub-sampled format is not supported, convert it to YUV444 or RGB first");
            return 1;
        }

        m = vsapi->propNumElements(in, "sigma");
        if (m > 0)
        {
            for (i = 0; i < m; i++)
            {
                sigma.push_back(vsapi->propGetFloat(in, "sigma", i, nullptr));

                if (sigma[i] < 0)
                {
                    setError(out, "Invalid \"sigma\" assigned, must be non-negative float number");
                    return 1;
                }
            }
        }
        else
        {
            sigma = MSRDefault.sigma;
        }

        size_t s, scount = sigma.size();

        for (s = 0; s < scount; s++)
        {
            if (sigma[s] > 0) break;
        }
        if (s >= scount)
        {
            for (i = 0; i < VSMaxPlaneCount; i++)
                process[i] = 0;
        }

        lower_thr = vsapi->propGetFloat(in, "lower_thr", 0, &error);
        if (error)
            lower_thr = MSRDefault.lower_thr;
        if (lower_thr < 0)
        {
            setError(out, "Invalid \"lower_thr\" assigned, must be float number ranges in [0, 1)");
            return 1;
        }

        upper_thr = vsapi->propGetFloat(in, "upper_thr", 0, &error);
        if (error)
            upper_thr = MSRDefault.upper_thr;
        if (upper_thr < 0)
        {
            setError(out, "Invalid \"upper_thr\" assigned, must be float number ranges in [0, 1)");
            return 1;
        }

        if (lower_thr + upper_thr >= 1)
        {
            setError(out, "Invalid \"lower_thr\" and \"upper_thr\" assigned, the sum of which mustn't equal or exceed 1");
            return 1;
        }

        fulls = vsapi->propGetInt(in, "fulls", 0, &error) == 0 ? false : true;
        if (error)
            fulls_select();

        fulld = vsapi->propGetInt(in, "fulld", 0, &error) == 0 ? false : true;
        if (error)
            fulld = fulls;

        return 0;
    }
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class MSRProcess
    : public VSProcess
{
private:
    const MSRData &d;

private:
    template < typename T >
    void process_core();

protected:
    virtual void process_core8() = 0;
    virtual void process_core16() = 0;

public:
    MSRProcess(const MSRData &_d, int n, VSFrameContext *frameCtx, VSCore *core, const VSAPI *_vsapi)
        : VSProcess(_d, n, frameCtx, core, _vsapi), d(_d) {}

    virtual ~MSRProcess() {}

    int SimplestColorBalance(FLType *odata, const FLType *idata) const;
    int MSRKernel(FLType *odata, const FLType *idata) const;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif