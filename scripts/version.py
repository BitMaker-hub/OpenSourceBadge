# Inyecta la versión y el commit en el binario, para saber qué lleva cada badge
# (sale al arrancar y en el comando `i`). La versión solo vive en platformio.ini
# (custom_version): el workflow de la release comprueba que la etiqueta coincide.
Import("env")
import subprocess

def git(*args):
    try:
        return subprocess.check_output(["git"] + list(args),
                                       stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return ""

commit = git("rev-parse", "--short=7", "HEAD") or "nogit"
if git("status", "--porcelain", "--untracked-files=no"):
    commit += "*"          # el árbol tenía cambios sin comitear

version = env.GetProjectOption("custom_version", "") or "dev"

env.Append(CPPDEFINES=[("OSB_VERSION", env.StringifyMacro(version)),
                       ("OSB_COMMIT",  env.StringifyMacro(commit))])
print("Version: %s  commit: %s" % (version, commit))
