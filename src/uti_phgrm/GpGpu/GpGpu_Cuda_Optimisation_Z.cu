#ifndef _OPTIMISATION_KERNEL_Z_H_
#define _OPTIMISATION_KERNEL_Z_H_

#include "GpGpu/GpGpu_StreamData.cuh"
#include "GpGpu/SData2Optimize.h"

// On pourrait imaginer un buffer des tailles calculer en parallel
// SIZEBUFFER[threadIdx.x] = count(lI[threadIdx.x]);

__device__ void GetConeZ(short2 & aDz, int aZ, int MaxDeltaZ, short2 aZ_Next, short2 aZ_Prev)
{
    aDz.x =   aZ_Prev.x-aZ;
    if (aZ != aZ_Next.x)
        aDz.x = max(aDz.x,-MaxDeltaZ);

    aDz.y = aZ_Prev.y-1-aZ;
    if (aZ != aZ_Next.y-1)
        aDz.y = min(aDz.y,MaxDeltaZ);

    if (aDz.x > aDz.y)
        if (aDz.y <0)
            aDz.x = aDz.y;
        else
            aDz.y = aDz.x;
}

template<bool sens> __device__
void RunLine(   SimpleStream<short2>    &streamIndex,
                SimpleStream<uint>      &streamFCost,
                SimpleStream<ushort>    &streamICost,
                short2     *S_Bf_Index,
                ushort     *ST_Bf_ICost,
                uint       *S_FCost[2],
                ushort     &sId_ICost,
                uint        penteMax,
                int        lenghtLine,
                short2     &prevIndex,
                int        &id_Line,
                short      &idSeg,
                bool       &idBuf
)
{
    const ushort  tid   = threadIdx.x;
    short2* ST_Bf_Index = S_Bf_Index + tid + (sens ? 0 : -WARPSIZE + 1);
    short2  ConeZ;
    uint    segLine     = 0;

    __shared__ uint globMinFCost;

    while(id_Line < lenghtLine)
    {

        segLine = min(lenghtLine-id_Line,WARPSIZE);

        while(idSeg < segLine)
        {

            const short2 index  = S_Bf_Index[sgn(idSeg)];
            const ushort dZ     = count(index); // creer buffer de count
            ushort       z      = 0;
            globMinFCost        = max_cost;

            //if(sens)
                while( z < dZ)
                {
                    if(sId_ICost > NAPPEMAX)
                    {
                        streamICost.read<sens>(ST_Bf_ICost);
                        streamFCost.incre<sens>();
                        sId_ICost = 0;
                    }

                    uint fCostMin           = max_cost;
                    const ushort costInit   = ST_Bf_ICost[sgn(sId_ICost)];
                    const ushort tZ         = z + tid;
                    const short  Z          = sens ? tZ + index.x : index.y - tZ;
                    const short pitPrZ      = sens ? Z - prevIndex.x : prevIndex.y - Z;

                    GetConeZ(ConeZ,Z,penteMax,index,prevIndex);

                    uint* prevFCost = S_FCost[idBuf] + sgn(pitPrZ);

                    ConeZ.y = min(NAPPEMAX - pitPrZ,ConeZ.y );

                    for (short i = ConeZ.x; i <= ConeZ.y; ++i)
                        fCostMin = min(fCostMin, costInit + prevFCost[i]); // ERROR

                    const uint fcost    =  fCostMin;// + sens * (streamFCost.GetValue(s_idCur_ICost) - costInit);

                    if( tZ < NAPPEMAX)
                    {
                        S_FCost[!idBuf][sgn(tZ)] = fcost;
                        //if(sens)
                        streamFCost.SetValue(sgn(sId_ICost), fcost); // ERROR

                        if(!sens)
                            atomicMin(&globMinFCost,fcost);
                    }

                    const ushort pIdCost = sId_ICost;
                    sId_ICost += min(dZ - z,WARPSIZE);
                    z         += min(WARPSIZE,NAPPEMAX-pIdCost);
                }

            prevIndex = index;
            idSeg++;
            idBuf =!idBuf;
        }

        id_Line += segLine;

        if(id_Line < lenghtLine)
            streamIndex.read<sens>(ST_Bf_Index);

        idSeg   = 0;
    }

    idSeg = segLine - 1;
}

template<class T> __global__
void Run(ushort* g_ICost, short2* g_Index, uint* g_FCost, uint3* g_RecStrParam, uint penteMax)
{
    __shared__ short2   S_BuffIndex[WARPSIZE];
    __shared__ ushort   S_BuffICost0[NAPPEMAX + 2*WARPSIZE];
    __shared__ uint     S_BuffFCost0[NAPPEMAX + 2*WARPSIZE];
    __shared__ uint     S_BuffFCost1[NAPPEMAX + 2*WARPSIZE];
    __shared__ uint     pit_Id;
    __shared__ uint     pit_Stream;
    __shared__ int      lenghtLine;

    const ushort    tid     = threadIdx.x;

    uint*    S_BuffFCost[2] = {S_BuffFCost0 + WARPSIZE,S_BuffFCost1 + WARPSIZE};
    ushort*  S_BuffICost    = S_BuffICost0 + WARPSIZE + tid;

    bool            idBuf   = false;
    ushort          s_id_Icost;

    if(!threadIdx.x)
    {
        uint3 recStrParam   = g_RecStrParam[blockIdx.x];
        pit_Stream          = recStrParam.x;
        pit_Id              = recStrParam.y;
        lenghtLine          = recStrParam.z;
    }

    __syncthreads();

    SimpleStream<ushort>    streamICost(g_ICost + pit_Stream,NAPPEMAX);
    SimpleStream<uint>      streamFCost(g_FCost + pit_Stream,NAPPEMAX);
    SimpleStream<short2>    streamIndex(g_Index + pit_Id    ,WARPSIZE);

    streamICost.read<eAVANT>(S_BuffICost);

    uint*   locFCost = S_BuffFCost[idBuf] + tid;
    for (ushort i = 0; i < NAPPEMAX; i+=WARPSIZE)
        locFCost[i] = S_BuffICost[i];

    streamIndex.read<eAVANT>(S_BuffIndex + tid);

    short2  prevIndex   = S_BuffIndex[0];
    int     id_Line     = 0;
    short   idSeg       = 1;

    s_id_Icost   = count(prevIndex);

    RunLine<eAVANT>(streamIndex,streamFCost,streamICost,S_BuffIndex,S_BuffICost,S_BuffFCost,s_id_Icost,penteMax,lenghtLine,prevIndex,id_Line,idSeg,idBuf);    

    streamIndex.reverse<eARRIERE>();
    streamFCost.reverse<eARRIERE>();

    S_BuffFCost[0]  += NAPPEMAX - WARPSIZE;
    S_BuffFCost[1]  += NAPPEMAX - WARPSIZE;
    S_BuffICost     += NAPPEMAX - WARPSIZE;

    streamICost.readFrom<eARRIERE>(S_BuffFCost[idBuf] + tid, NAPPEMAX - s_id_Icost);

    streamICost.reverse<eARRIERE>();

    prevIndex       = S_BuffIndex[idSeg];
    idSeg           = WARPSIZE - idSeg;
    id_Line         = 1;

    const short noRead   = count(prevIndex) - s_id_Icost;

    if(noRead < 0)
        s_id_Icost = NAPPEMAX + noRead;
    else
    {
        streamICost.read<eARRIERE>(S_BuffICost);
        streamFCost.incre<eARRIERE>();
        s_id_Icost = noRead;
    }

    RunLine<eARRIERE>(  streamIndex,
                        streamFCost,
                        streamICost,
                        S_BuffIndex + WARPSIZE - 1,
                        S_BuffICost,
                        S_BuffFCost,
                        s_id_Icost,
                        penteMax,
                        lenghtLine + idSeg,
                        prevIndex,
                        id_Line,
                        idSeg,
                        idBuf);

}

extern "C" void OptimisationOneDirectionZ(Data2Optimiz<CuDeviceData3D> &d2O)
{
    uint deltaMax = 3;
    dim3 Threads(WARPSIZE,1,1);
    dim3 Blocks(d2O.NBlines(),1,1);

    Run< uint ><<<Blocks,Threads>>>
                                  (
                                      d2O.pInitCost(),
                                      d2O.pIndex(),
                                      d2O.pForceCostVol(),
                                      d2O.pParam(),
                                      deltaMax
                                      );
    getLastCudaError("kernelOptiOneDirection failed");
}

#endif //_OPTIMISATION_KERNEL_Z_H_

