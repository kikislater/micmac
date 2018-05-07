#include "StdAfx.h"

std::string TheTypeGCP2DConv = "XXXX";
#define aNbTypeGCP2DConv 3
std::string  TypeGCP2DConv[aNbTypeGCP2DConv] = {"MM2txt","Pix4D2MM","Appui2Homol"};
std::string aFileOut,aFileIn;
int convertGCPSaisie_Pix4D2MM(int argc,char ** argv);
int exportGCP2DMes2txt(int argc,char ** argv);
// goal: convert 2D measure of appuis (from 'saisie appuis' tools ) to homol format
class cAppuis2Homol
{
public:
    cAppuis2Homol(int argc, char** argv);
private:
    cInterfChantierNameManipulateur * mICNM;
    bool mDebug,mExpTxt;
    std::string mIm1,mIm2,mHomPackOut, mSH, m2DMesFileName;
};

int GCP2DMeasureConvert_main(int argc,char ** argv)
{

    ELISE_ASSERT(argc >= 2,"Not enough arg");
    TheTypeGCP2DConv = argv[1];

    MMD_InitArgcArgv(argc,argv);

    int ARGC0 = argc;

    //  APRES AVOIR SAUVEGARDER L'ARGUMENT DE TYPE ON LE SUPPRIME
    if (argc>=2)
    {
        argv[1] = argv[0];
        argv++; argc--;
    }

    if (TheTypeGCP2DConv == TypeGCP2DConv[0])
    {
        int aRes = exportGCP2DMes2txt(argc, argv);
        return aRes;
    }
    else if (TheTypeGCP2DConv == TypeGCP2DConv[1])
    {
        int aRes = convertGCPSaisie_Pix4D2MM(argc, argv);
        return aRes;
    }
    else if (TheTypeGCP2DConv == TypeGCP2DConv[2])
    {
        cAppuis2Homol(argc,argv);
        return EXIT_SUCCESS;
    }

    bool Error = (ARGC0>=2 ) && (TheTypeGCP2DConv!= std::string("-help"));
    if (Error)
    {
        std::cout << "GCP2DMeasureConvert: ERROR: unknown command : " << TheTypeGCP2DConv << endl;
    }
    std::cout << "Allowed commands are : \n";
    for (int aK=0 ; aK<aNbTypeGCP2DConv ; aK++)
        std::cout << "\t" << TypeGCP2DConv[aK] << "\n";
    if (!Error)
    {
        std::cout << "for details : \n";
        std::cout << "\t GCP2DMeasureConvert MM2txt -help\n";
        std::cout << "\t GCP2DMeasureConvert Pix4D2MM -help\n";
        std::cout << "\t...\n";
    }

    return EXIT_FAILURE;
}

int convertGCPSaisie_Pix4D2MM(int argc,char ** argv)
{
    std::string aPat;
    bool aNameDup(0); // for specific case, like the one with 2 flights of camlight or dji uas, that end up with two (or more) images blocs that have the same image name!
    //In this very case Pix4D attribute an Id Label to the image instead of the name. The label is as following: Directory of image/(id).JPG
    std::vector<std::string> aDirFlight;
    std::map<std::string,std::string> KAIm; // associate image label of Pix4D with image dir+name

    ElInitArgMain
            (
                argc,argv,
                LArgMain()
                << EAMC(aFileIn,"txt file from Pix4D soft generated by 'Export Image Marks' process, export format 'Pix4D marks files'",eSAM_IsExistFile),
                LArgMain()  << EAM(aFileOut,"Out",true, "Resulting xml file, 2D-Measurement of GCP in micmac format.")
                << EAM(aPat,"Pat",true, "Image Pattern, to use ONLY in case of several block sharing images that have the same name. In that case images are indexed (id=) by Pix4D.", eSAM_IsPatFile)
                << EAM(aDirFlight,"MulDir",true, "[DirFlight1,DirFlight2,..] List of directory of differents flights, case several block share same image name", eSAM_NoInit)
                );

    if (!MMVisualMode)
    {


        if (EAMIsInit(&aPat) && EAMIsInit(&aDirFlight))
        {
            aNameDup=1;
            std::cout << "Warn, process " << aDirFlight.size() <<"image blocs that are located under different directory.\n";

            for (auto & dir : aDirFlight){
                if (ELISE_fp::IsDirectory(dir))
                {

                    cInterfChantierNameManipulateur * aICNM= cInterfChantierNameManipulateur::BasicAlloc(dir);
                    const std::vector<std::string> aSetIm = *(aICNM->Get(aPat));
                    int it(1);
                    for (auto & im : aSetIm){
                        std::string pix4Dlab=dir+" ("+ToString(it+1)+").JPG";
                        KAIm[pix4Dlab]=dir+im;
                        std::cout << "Pix4D label " << pix4Dlab << " is pointing to image " << KAIm[pix4Dlab] <<"\n";

                        it++;
                    }
                } else { std::cout << "cannot find directory " << dir << ", I skip it.\n";}
            }
        }

        if ((!EAMIsInit(&aPat) && EAMIsInit(&aDirFlight)) || (EAMIsInit(&aPat) && !EAMIsInit(&aDirFlight) )) {
            std::cout << "Warn, multi images blocs option require use of both 'Pat' and 'MulDir'. Abording\n";
            return EXIT_FAILURE;
        }

        if (!EAMIsInit(&aFileOut)) aFileOut=aFileIn.substr(0,aFileIn.size()-4)+"_MM.xml";

        if (ELISE_fp::exist_file(aFileIn))
        {
            ifstream aFile(aFileIn.c_str());
            if(aFile)
            {
                cSetOfMesureAppuisFlottants MAF;
                std::string aLine;

                while(!aFile.eof())
                {
                    getline(aFile,aLine,'\n');

                    if(aLine.size() != 0)
                    {
                        // read line information: Im_Label, GCP_label,U,V   ,DeZoomForMarking,Path2Images
                        char *aBuffer = strdup((char*)aLine.c_str());
                        std::string aImLab = strtok(aBuffer,","); // im lab or im name
                        std::string aGCPLab = strtok( NULL, "," );
                        std::string aU = strtok( NULL, "," );
                        std::string aV = strtok( NULL, "," );

                        if (aNameDup)
                        {
                            if (KAIm.count(aImLab)>0)
                            {aImLab=KAIm[aImLab];} else {std::cout << " cannot link image label " << aImLab << " to an im Name, sorry \n";}
                        }

                        double U,V;
                        FromString(U,aU);
                        FromString(V,aV);

                        // std::cout << "Image name : " << aImLab << " GCP label " << aGCPLab << " U,V: " << U << "," << V << std::endl;

                        // add these to the result
                        cMesureAppuiFlottant1Im aMark;
                        aMark.NameIm()=aImLab;
                        cOneMesureAF1I currentMAF;
                        currentMAF.NamePt()=aGCPLab;
                        currentMAF.PtIm()=Pt2dr(U,V);
                        aMark.OneMesureAF1I().push_back(currentMAF);
                        MAF.MesureAppuiFlottant1Im().push_back(aMark);
                    }
                }
                aFile.close();
                MakeFileXML(MAF,aFileOut);
                std::cout << "Export GCP mark to micmac format, file " << aFileOut << "\n";
            }
            return EXIT_SUCCESS;
        } else {
            std::cout << "Error, cannot find file " << aFileIn << "\n";
            return EXIT_FAILURE;
        }
        // end if MMVisual mode
    }
    return 0;
}


int exportGCP2DMes2txt(int argc,char ** argv)
{
    ElInitArgMain
            (
                argc,argv,
                LArgMain()
                << EAMC(aFileIn,"xml file of 2D measures in micmac format, as generated by SaisieAppui tools'",eSAM_IsExistFile),
                LArgMain()  << EAM(aFileOut,"Out",true, "Resulting txt file, 2D-Measurement of GCP in txt format.")
                );

    if (!MMVisualMode)
    {
        if (ELISE_fp::exist_file(aFileIn)){

            // name of output
            if (!EAMIsInit(&aFileOut)) aFileOut=aFileIn.substr(0,aFileIn.size()-3)+"txt";
            // open txt output
            FILE * aFOut = FopenNN(aFileOut.c_str(),"w","out");
            fprintf(aFOut,"ImageName GCPName U V markPrecision\n");
            // read input xml file
            cSetOfMesureAppuisFlottants MAF=StdGetFromPCP(aFileIn,SetOfMesureAppuisFlottants);

            for( std::list< cMesureAppuiFlottant1Im >::const_iterator iTmes1Im=MAF.MesureAppuiFlottant1Im().begin();
                 iTmes1Im!=MAF.MesureAppuiFlottant1Im().end();          iTmes1Im++    )
            {
                cMesureAppuiFlottant1Im MAF1im=*iTmes1Im;


                for( std::list< cOneMesureAF1I >::const_iterator iT=MAF1im.OneMesureAF1I().begin();
                     iT!=MAF1im.OneMesureAF1I().end();          iT++    )

                {
                    cOneMesureAF1I gcp=*iT;

                    fprintf(aFOut,"%s %s %f %f %f\n",MAF1im.NameIm().c_str(),gcp.NamePt().c_str(), gcp.PtIm().x, gcp.PtIm().y, gcp.PrecPointe());
                }
            }

        } else {
            std::cout << "I cannot find file " << aFileIn << ", abording.\n";
            return EXIT_FAILURE;
        }

    }
    return 0;
}



cAppuis2Homol::cAppuis2Homol(int argc, char** argv):
    mDebug(0),
    mExpTxt(0),
    mSH("-Appui")

{

    ElInitArgMain
            (
                argc,argv,
                LArgMain()  << EAMC(mIm1, "Image 1 name",eSAM_IsExistFile)
                << EAMC(mIm2, "Image 2 name",eSAM_IsExistFile)
                << EAMC(m2DMesFileName, "  2D measures of GCPs, as results of SaisieAppuiInit ",eSAM_IsExistFile),
                LArgMain()
                << EAM(mDebug,"Debug",true,"Print Messages to help debugging process")
                << EAM(mSH, "SH", true, "Set of Homol postfix, def '-Appui' will write homol to Homol-Appui/ directory")
                << EAM(mExpTxt,"ExpTxt",true,"Save homol as text? default false, mean binary format")
                );

    mICNM=cInterfChantierNameManipulateur::BasicAlloc("./");
    std::string aExt("dat");
    if (mExpTxt) aExt="txt";

    // initialiser le pack de points homologues
    ElPackHomologue  aPackHom;
    if (mDebug) std::cout << "open 2D mesures\n";
    cSetOfMesureAppuisFlottants aSetOfMesureAppuisFlottants=StdGetFromPCP(m2DMesFileName,SetOfMesureAppuisFlottants);
    if (mDebug) std::cout << "Done\n";
       int count(0);
    for (auto &aMesAppuisIm1 : aSetOfMesureAppuisFlottants.MesureAppuiFlottant1Im())
    {
        if(aMesAppuisIm1.NameIm() == mIm1)
        {
            if (mDebug) std::cout << "Found 2D mesures for Image 1\n";
            for (auto &aMesAppuisIm2 : aSetOfMesureAppuisFlottants.MesureAppuiFlottant1Im())
            {
                if(aMesAppuisIm2.NameIm() == mIm2)
                {
                     if (mDebug) std::cout << "Found 2D mesures for Image 2\n";

                    for (auto & OneAppuiIm1 : aMesAppuisIm1.OneMesureAF1I())
                    {

                        for (auto & OneAppuiIm2 : aMesAppuisIm2.OneMesureAF1I())
                        {
                            if (OneAppuiIm1.NamePt()==OneAppuiIm2.NamePt())
                            {
                                ElCplePtsHomologues Homol(OneAppuiIm1.PtIm(),OneAppuiIm2.PtIm());

                                aPackHom.Cple_Add(Homol);

                                count++;
                            }
                        }
                    }
                 break;
                }
            }
        break;
        }
    }

    // save result
    if (mDebug) std::cout << "Save Results\n";

    if (count!=0)
    {
    std::string aKeyAsocHom = "NKS-Assoc-CplIm2Hom@"+ mSH +"@" + aExt;
    if (mDebug) std::cout << "NKS " << aKeyAsocHom << "\n";
    std::string aHomolFile= mICNM->Assoc1To2(aKeyAsocHom, mIm1, mIm2,true);
    if (mDebug) std::cout << "generate " << aHomolFile << "\n";

    aPackHom.StdPutInFile(aHomolFile);
    aPackHom.SelfSwap();
    aHomolFile=  mICNM->Assoc1To2(aKeyAsocHom, mIm2, mIm1,true);
    if (mDebug) std::cout << "generate " << aHomolFile << "\n";

    aPackHom.StdPutInFile(aHomolFile);
    std::cout << "Finished, " << count << " manual seasing (marks) of GCP have been converted in homol format\n";
    std::string aKH("NB");
    if (mExpTxt) aKH="NT";

    std::cout << "Launch SEL for visualisation: SEL ./ " << mIm1 << " " << mIm2 << " KH=" << aKH << " SH=" << mSH << "\n";
    } else { std::cout << "I haven't found couple of GCP 2D measures for these images pairs, sorry \n";}
}

int GCP2Hom_main(int argc,char ** argv)
{
   cAppuis2Homol(argc,argv);
   return EXIT_SUCCESS;
}

