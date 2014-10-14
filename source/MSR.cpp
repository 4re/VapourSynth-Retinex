#include <cmath>
#include <vapoursynth\VSHelper.h>
#include "..\include\Helper.h"
#include "..\include\Gaussian.h"
#include "..\include\MSR.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int Retinex_MSR(FLType *odata, const FLType *idata, const MSRData &d, int height, int width, int stride)
{
    size_t s, scount = d.sigma.size();

    int i, j, upper;
    int pcount = stride * height;

    int Floor = 0;
    int Ceil = 1;
    FLType FloorFL = static_cast<FLType>(Floor);
    FLType CeilFL = static_cast<FLType>(Ceil);

    FLType *gauss = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);

    for (j = 0; j < height; j++)
    {
        i = stride * j;
        for (upper = i + width; i < upper; i++)
            odata[i] = 1;
    }

    FLType B, B1, B2, B3;

    for (s = 0; s < scount; s++)
    {
        if (d.sigma[s] > 0)
        {
            Recursive_Gaussian_Parameters(d.sigma[s], B, B1, B2, B3);
            Recursive_Gaussian2D_Horizontal(gauss, idata, height, width, stride, B, B1, B2, B3);
            Recursive_Gaussian2D_Vertical(gauss, gauss, height, width, stride, B, B1, B2, B3);

            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    odata[i] *= gauss[i] <= 0 ? 1 : idata[i] / gauss[i] + 1;
            }
        }
        else
        {
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    odata[i] *= FLType(2);
            }
        }
    }

    for (j = 0; j < height; j++)
    {
        i = stride * j;
        for (upper = i + width; i < upper; i++)
            odata[i] = std::log(odata[i]) / static_cast<FLType>(scount);
    }

    vs_aligned_free(gauss);

    FLType offset, gain;
    FLType min = FLType_MAX;
    FLType max = -FLType_MAX;

    for (j = 0; j < height; j++)
    {
        i = stride * j;
        for (upper = i + width; i < upper; i++)
        {
            min = Min(min, odata[i]);
            max = Max(max, odata[i]);
        }
    }

    if (max <= min)
    {
        memcpy(odata, idata, sizeof(FLType)*pcount);
        return 1;
    }

    if (d.lower_thr > 0 || d.upper_thr > 0)
    {
        size_t h, HistBins = d.HistBins;
        int Count, MaxCount;

        int *Histogram = vs_aligned_malloc<int>(sizeof(int)*HistBins, Alignment);
        memset(Histogram, 0, sizeof(int)*HistBins);

        gain = (HistBins - 1) / (max - min);
        offset = FLType(0.5) - min * gain;

        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
            {
                Histogram[static_cast<int>(odata[i] * gain + offset)]++;
            }
        }

        gain = (max - min) / (HistBins - 1);
        offset = FLType(0.5) + min;

        Count = 0;
        MaxCount = static_cast<int>(width*height*d.lower_thr + 0.5);

        for (h = 0; h < HistBins; h++)
        {
            Count += Histogram[h];
            if (Count > MaxCount) break;
        }

        min = h * gain + offset;

        Count = 0;
        MaxCount = static_cast<int>(width*height*d.upper_thr + 0.5);

        for (h = HistBins - 1; h >= 0; h--)
        {
            Count += Histogram[h];
            if (Count > MaxCount) break;
        }

        max = h * gain + offset;

        vs_aligned_free(Histogram);
    }

    gain = (CeilFL - FloorFL) / (max - min);
    offset = -min * gain + FloorFL;

    for (j = 0; j < height; j++)
    {
        i = stride * j;
        for (upper = i + width; i < upper; i++)
            odata[i] = odata[i] * gain + offset;
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////