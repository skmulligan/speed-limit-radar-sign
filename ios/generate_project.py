"""Generate the dependency-free iOS Xcode project. Run in Conda cryoem."""
from pathlib import Path
import hashlib
ROOT = Path(__file__).resolve().parent

def ident(label):
    return hashlib.sha256(label.encode()).hexdigest()[:24].upper()


def main():
    ids = {key: ident(key) for key in ('project', 'main', 'sources', 'products', 'app', 'target', 'sourcePhase', 'frameworkPhase', 'resourcePhase', 'projectConfigs', 'targetConfigs', 'package', 'core', 'coreBuild')}
    ids.update({key: ident(key) for key in ('testGroup','testFile','testBuild','testSources','testTarget','testProduct','testConfigs','testProxy','testDependency')})
    objects = []
    def add(key, value):
        objects.append(f'{ids.get(key, ident(key))} = {{ {value} }};')
    files = sorted((ROOT / 'SpeedDetector').glob('*.swift'))
    for file in files:
        add(file.name, f'isa = PBXFileReference; lastKnownFileType = sourcecode.swift; path = "{file.name}"; sourceTree = "<group>";')
        add('build'+file.name, f'isa = PBXBuildFile; fileRef = {ident(file.name)};')
    add('main', f'isa = PBXGroup; children = ({ids["sources"]}, {ids["testGroup"]}, {ids["products"]}); sourceTree = "<group>";')
    add('Assets.xcassets', 'isa = PBXFileReference; lastKnownFileType = folder.assetcatalog; path = Assets.xcassets; sourceTree = "<group>";')
    add('buildAssets.xcassets', f'isa = PBXBuildFile; fileRef = {ident("Assets.xcassets")};')
    add('sources', f'isa = PBXGroup; path = SpeedDetector; children = ({",".join(ident(f.name) for f in files)},{ident("Assets.xcassets")}); sourceTree = "<group>";')
    add('products', f'isa = PBXGroup; name = Products; children = ({ids["app"]},{ids["testProduct"]}); sourceTree = "<group>";')
    add('app', 'isa = PBXFileReference; explicitFileType = wrapper.application; path = "Street Speed.app"; sourceTree = BUILT_PRODUCTS_DIR;')
    add('sourcePhase', f'isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ({",".join(ident("build"+f.name) for f in files)}); runOnlyForDeploymentPostprocessing = 0;')
    add('frameworkPhase', f'isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = ({ids["coreBuild"]}); runOnlyForDeploymentPostprocessing = 0;')
    add('resourcePhase', f'isa = PBXResourcesBuildPhase; buildActionMask = 2147483647; files = ({ident("buildAssets.xcassets")}); runOnlyForDeploymentPostprocessing = 0;')
    add('package', 'isa = XCLocalSwiftPackageReference; relativePath = SpeedDetectorCore;')
    add('core', f'isa = XCSwiftPackageProductDependency; package = {ids["package"]}; productName = SpeedDetectorCore;')
    add('coreBuild', f'isa = PBXBuildFile; productRef = {ids["core"]};')
    add('target', f'isa = PBXNativeTarget; buildConfigurationList = {ids["targetConfigs"]}; buildPhases = ({ids["sourcePhase"]},{ids["frameworkPhase"]},{ids["resourcePhase"]}); buildRules = (); dependencies = (); name = StreetSpeed; productName = "Street Speed"; productReference = {ids["app"]}; productType = "com.apple.product-type.application"; packageProductDependencies = ({ids["core"]});')
    add('project', f'isa = PBXProject; attributes = {{ LastUpgradeCheck = 1600; }}; buildConfigurationList = {ids["projectConfigs"]}; compatibilityVersion = "Xcode 14.0"; developmentRegion = en; knownRegions = (en,Base); mainGroup = {ids["main"]}; productRefGroup = {ids["products"]}; projectDirPath = ""; projectRoot = ""; targets = ({ids["target"]},{ids["testTarget"]}); packageReferences = ({ids["package"]});')
    add('testGroup',f'isa = PBXGroup; path = SpeedDetectorUITests; children = ({ids["testFile"]}); sourceTree = "<group>";')
    add('testFile','isa = PBXFileReference; lastKnownFileType = sourcecode.swift; path = NavigationTests.swift; sourceTree = "<group>";')
    add('testBuild',f'isa = PBXBuildFile; fileRef = {ids["testFile"]};')
    add('testSources',f'isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ({ids["testBuild"]}); runOnlyForDeploymentPostprocessing = 0;')
    add('testProduct','isa = PBXFileReference; explicitFileType = wrapper.cfbundle; path = StreetSpeedUITests.xctest; sourceTree = BUILT_PRODUCTS_DIR;')
    add('testProxy',f'isa = PBXContainerItemProxy; containerPortal = {ids["project"]}; proxyType = 1; remoteGlobalIDString = {ids["target"]}; remoteInfo = StreetSpeed;')
    add('testDependency',f'isa = PBXTargetDependency; target = {ids["target"]}; targetProxy = {ids["testProxy"]};')
    add('testTarget',f'isa = PBXNativeTarget; name = StreetSpeedUITests; productName = StreetSpeedUITests; productType = "com.apple.product-type.bundle.ui-testing"; productReference = {ids["testProduct"]}; buildConfigurationList = {ids["testConfigs"]}; buildPhases = ({ids["testSources"]}); dependencies = ({ids["testDependency"]}); buildRules = ();')
    for mode in ('Debug','Release'):
        add('test'+mode,f'isa = XCBuildConfiguration; name = {mode}; buildSettings = {{ PRODUCT_NAME = StreetSpeedUITests; PRODUCT_BUNDLE_IDENTIFIER = org.skmulligan.StreetSpeedUITests; GENERATE_INFOPLIST_FILE = YES; TEST_TARGET_NAME = StreetSpeed; CODE_SIGN_STYLE = Automatic; TARGETED_DEVICE_FAMILY = "1,2"; }};')
        add('project'+mode, f'isa = XCBuildConfiguration; name = {mode}; buildSettings = {{ SDKROOT = iphoneos; IPHONEOS_DEPLOYMENT_TARGET = 17.0; CLANG_ENABLE_MODULES = YES; SWIFT_VERSION = 5.0; ONLY_ACTIVE_ARCH = {"YES" if mode == "Debug" else "NO"}; SWIFT_OPTIMIZATION_LEVEL = "{"-Onone" if mode == "Debug" else "-O"}"; }};')
        add('target'+mode, f'''isa = XCBuildConfiguration; name = {mode}; buildSettings = {{
            PRODUCT_NAME = "Street Speed"; PRODUCT_BUNDLE_IDENTIFIER = org.skmulligan.StreetSpeed;
            GENERATE_INFOPLIST_FILE = YES; INFOPLIST_KEY_CFBundleDisplayName = "Street Speed";
            ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon;
            INFOPLIST_KEY_NSBluetoothAlwaysUsageDescription = "Connect to your speed sign to view live radar and download detection history.";
            INFOPLIST_KEY_UILaunchScreen_Generation = YES; INFOPLIST_KEY_UIApplicationSceneManifest_Generation = YES;
            INFOPLIST_KEY_UISupportedInterfaceOrientations = "UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight";
            CODE_SIGN_STYLE = Automatic; CURRENT_PROJECT_VERSION = 1; MARKETING_VERSION = 0.1.0;
            TARGETED_DEVICE_FAMILY = "1,2"; SUPPORTED_PLATFORMS = "iphoneos iphonesimulator";
            SWIFT_EMIT_LOC_STRINGS = YES; LD_RUNPATH_SEARCH_PATHS = "$(inherited) @executable_path/Frameworks";
        }};''')
    for scope in ('project','target','test'):
        add(scope+'Configs',f'isa = XCConfigurationList; buildConfigurations = ({ident(scope+"Debug")},{ident(scope+"Release")}); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release;')
    project = ROOT / 'StreetSpeed.xcodeproj'
    project.mkdir(exist_ok=True)
    (project/'project.pbxproj').write_text('// !$*UTF8*$!\n{ archiveVersion = 1; classes = {}; objectVersion = 56; objects = {\n'+'\n'.join(objects)+f'\n}}; rootObject = {ids["project"]}; }}\n')
    schemes = project/'xcshareddata/xcschemes'; schemes.mkdir(parents=True,exist_ok=True)
    (schemes/'StreetSpeed.xcscheme').write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.3"><BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{ids['target']}" BuildableName="Street Speed.app" BlueprintName="StreetSpeed" ReferencedContainer="container:StreetSpeed.xcodeproj"/></BuildActionEntry></BuildActionEntries></BuildAction><TestAction buildConfiguration="Debug"/><LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{ids['target']}" BuildableName="Street Speed.app" BlueprintName="StreetSpeed" ReferencedContainer="container:StreetSpeed.xcodeproj"/></BuildableProductRunnable></LaunchAction><ProfileAction buildConfiguration="Release"/><AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/></Scheme>''')
    scheme=schemes/'StreetSpeed.xcscheme'
    scheme.write_text(scheme.read_text().replace('<TestAction buildConfiguration="Debug"/>',f'<TestAction buildConfiguration="Debug"><Testables><TestableReference skipped="NO"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{ids["testTarget"]}" BuildableName="StreetSpeedUITests.xctest" BlueprintName="StreetSpeedUITests" ReferencedContainer="container:StreetSpeed.xcodeproj"/></TestableReference></Testables></TestAction>'))

if __name__ == '__main__':
    main()
