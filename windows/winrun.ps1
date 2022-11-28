$archive = 'linux-5.18.0-rv32nommu-cnl-1.zip';

if( !(Test-Path -Path Image) )
{
	Invoke-WebRequest -Uri https://github.com/cnlohr/mini-rv32ima-images/raw/master/images/$archive -UseBasicParsing -OutFile $archive;
	Expand-Archive linux-5.18.0-rv32nommu-cnl-1.zip -DestinationPath . -ErrorAction SilentlyContinue;
}

$compiler = "tcc";

if( !(Get-Command $compiler -ErrorAction Stop) )
{
	Write-Host 'No TCC.  Please get it with installer here: https://github.com/cnlohr/tinycc-win64-installer or by powershell here: https://github.com/cntools/Install-TCC';
}

& $compiler ..\mini-rv32ima\mini-rv32ima.c

if( $? )
{
	.\mini-rv32ima.exe -f Image
}


