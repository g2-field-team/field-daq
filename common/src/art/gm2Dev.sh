#Define basic info
OFFLINE_DIR=/home/newg2/gm2Dev                          #Root directory for g-2 offline software packages
VERSION=v7_04_00                                                #g-2 software version
WORKSPACE=$OFFLINE_DIR/gm2Dev_${VERSION}                        #Path to your workspace
FLAG=prof                                                       #Version flag

#Use ninja
setup ninja v1_6_0b
alias makeninja='pushd $MRB_BUILDDIR ; ninja ; popd'

#Create workspace directory ifs required
if [ ! -d "$WORKSPACE" ]; then
echo "" 
echo "Creating the directory \"$WORKSPACE\"" 
mkdir -p $WORKSPACE
fi

#If workspace has not already been initialised, do it now
DEVELOPAREA=$WORKSPACE/localProducts_gm2_${VERSION}_${FLAG}
if [ ! -d "$DEVELOPAREA" ]; then
echo "" 
echo "Creating a new workspace in \"$WORKSPACE\"" 
cd $WORKSPACE
mrb newDev                      #Create a new workspace
fi

#Setup workspace environment
echo "" 
echo "Setup workspace" 
cd $WORKSPACE
source localProducts_gm2_${VERSION}_prof/setup    #Setup mrb for this workspace
source mrb setEnv                                 #Set build environment for this workspace

#MIDAS (create UPS product stub)
GM2MIDAS_DIR=/home/newg2/Packages/gm2midas
product-stub gm2midas v0_00_01 $GM2MIDAS_DIR $MRB_INSTALL

#Done
echo "" 
echo "Setup complete!" 
echo "Workspace : $WORKSPACE" 
echo "MIDAS : $GM2MIDAS_DIR" 
echo "" 
