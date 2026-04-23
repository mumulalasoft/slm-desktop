.pragma library
.import "ShellUtils.js" as ShellUtils

var _pulseCalculatorLastValue = 0

function _appModel(shell) {
    if (shell && shell.appModelRef) {
        return shell.appModelRef
    }
    if (typeof AppModel !== "undefined" && AppModel) {
        return AppModel
    }
    return null
}

function _appDeckModel(shell) {
    if (shell && shell.appDeckModelRef) {
        return shell.appDeckModelRef
    }
    if (typeof AppDeckModel !== "undefined" && AppDeckModel) {
        return AppDeckModel
    }
    return null
}

function _fileApi(shell) {
    if (shell && shell.fileManagerApiRef) {
        return shell.fileManagerApiRef
    }
    if (typeof FileManagerApi !== "undefined" && FileManagerApi) {
        return FileManagerApi
    }
    return null
}

function _pulseService(shell) {
    if (shell && shell.pulseServiceRef) {
        return shell.pulseServiceRef
    }
    if (typeof PulseService !== "undefined" && PulseService) {
        return PulseService
    }
    return null
}

function _pulseSettingBool(path, fallback) {
    if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
        return !!fallback
    }
    var value = DesktopSettings.settingValue(path, fallback)
    if (typeof value === "boolean") {
        return value
    }
    if (typeof value === "number") {
        return value !== 0
    }
    var normalized = String(value).trim().toLowerCase()
    return normalized === "1"
            || normalized === "true"
            || normalized === "yes"
            || normalized === "on"
}

function _pulseSettingInt(path, fallback, minValue, maxValue) {
    if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
        return Number(fallback || 0)
    }
    var raw = DesktopSettings.settingValue(path, fallback)
    var value = Number(raw)
    if (isNaN(value)) {
        value = Number(fallback || 0)
    }
    value = Math.floor(value)
    if (typeof minValue === "number" && value < minValue) {
        value = minValue
    }
    if (typeof maxValue === "number" && value > maxValue) {
        value = maxValue
    }
    return value
}

function _pulseCalculatorAliasLookup(value, maps) {
    for (var i = 0; i < maps.length; ++i) {
        var map = maps[i]
        if (map && Object.prototype.hasOwnProperty.call(map, value)) {
            return String(map[value] || "")
        }
    }
    return ""
}

function _pulseCalculatorFactorLookup(value, map) {
    if (!map || !Object.prototype.hasOwnProperty.call(map, value)) {
        return null
    }
    return Number(map[value])
}

var _pulseCalculatorSpeedUnits = ({
    "m/s": true,
    "km/h": true,
    "mph": true,
    "kph": true,
    "kmh": true,
    "mps": true,
    "fps": true,
    "knot": true,
    "kn": true,
    "kt": true
})

var _pulseCalculatorFrequencyUnits = ({
    "hz": true,
    "khz": true,
    "mhz": true,
    "ghz": true,
    "thz": true,
    "rpm": true
})

var _pulseCalculatorRatioUnits = ({
    "pct": true,
    "ratio": true
})

var _pulseCalculatorSpeedFactors = ({
    "m/s": 1,
    "km/h": 1000 / 3600,
    "mph": 1609.344 / 3600,
    "kph": 1000 / 3600,
    "kmh": 1000 / 3600,
    "mps": 1,
    "fps": 0.3048,
    "knot": 1852 / 3600,
    "kn": 1852 / 3600,
    "kt": 1852 / 3600
})

var _pulseCalculatorFrequencyFactors = ({
    "hz": 1,
    "khz": 1000,
    "mhz": 1000 * 1000,
    "ghz": 1000 * 1000 * 1000,
    "thz": 1000 * 1000 * 1000 * 1000,
    "rpm": 1 / 60
})

var _pulseCalculatorRatioFactors = ({
    "pct": 0.01,
    "ratio": 1
})

var _pulseCalculatorTemperatureUnits = ({
    "c": true,
    "f": true,
    "k": true,
    "r": true,
    "re": true
})

var _pulseCalculatorAngleUnits = ({
    "deg": true,
    "rad": true,
    "grad": true,
    "turn": true
})

var _pulseCalculatorAngularSpeedUnits = ({
    "deg/s": true,
    "rad/s": true,
    "turn/s": true
})

var _pulseCalculatorAngularSpeedFactors = ({
    "deg/s": Math.PI / 180,
    "rad/s": 1,
    "turn/s": Math.PI * 2
})

var _pulseCalculatorAngleFactors = ({
    "deg": Math.PI / 180,
    "rad": 1,
    "grad": Math.PI / 200,
    "turn": Math.PI * 2
})

var _pulseCalculatorPressureFactors = ({
    "pa": 1,
    "kpa": 1000,
    "mpa": 1000 * 1000,
    "bar": 100000,
    "atm": 101325,
    "psi": 6894.757293168,
    "torr": 133.32236842105263,
    "mmhg": 133.32236842105263,
    "inhg": 3386.389
})

var _pulseCalculatorEnergyFactors = ({
    "j": 1,
    "kj": 1000,
    "mj": 1000 * 1000,
    "wh": 3600,
    "kwh": 3600 * 1000,
    "cal": 4.184,
    "kcal": 4184,
    "btu": 1055.05585262
})

var _pulseCalculatorPowerFactors = ({
    "w": 1,
    "kw": 1000,
    "mw": 0.001,
    "hp": 745.6998715822702,
    "j/s": 1
})

var _pulseCalculatorForceFactors = ({
    "n": 1,
    "kn": 1000,
    "dyn": 0.00001,
    "lbf": 4.4482216152605,
    "kgf": 9.80665
})

var _pulseCalculatorAccelerationFactors = ({
    "m/s2": 1,
    "cm/s2": 0.01,
    "mm/s2": 0.001,
    "ft/s2": 0.3048,
    "km/h2": 1000 / (3600 * 3600)
})

var _pulseCalculatorDensityFactors = ({
    "kg/m3": 1,
    "g/cm3": 1000,
    "g/ml": 1000,
    "kg/l": 1000,
    "lb/ft3": 16.01846337396014
})

var _pulseCalculatorFlowFactors = ({
    "m3/s": 1,
    "l/s": 0.001,
    "ml/s": 0.000001,
    "kg/s": 1,
    "g/s": 0.001
})

var _pulseCalculatorDataRateUnits = ({
    "bps": true,
    "kbps": true,
    "mbps": true,
    "gbps": true,
    "tbps": true,
    "b/s": true,
    "kb/s": true,
    "mb/s": true,
    "gb/s": true,
    "tb/s": true,
    "kib/s": true,
    "mib/s": true,
    "gib/s": true,
    "tib/s": true,
    "B/s": true,
    "KB/s": true,
    "MB/s": true,
    "GB/s": true,
    "TB/s": true,
    "KiB/s": true,
    "MiB/s": true,
    "GiB/s": true,
    "TiB/s": true
})

var _pulseCalculatorDataRateFactors = ({
    "bps": 1,
    "kbps": 1000,
    "mbps": 1000 * 1000,
    "gbps": 1000 * 1000 * 1000,
    "tbps": 1000 * 1000 * 1000 * 1000,
    "b/s": 1,
    "kb/s": 8 * 1000,
    "mb/s": 8 * 1000 * 1000,
    "gb/s": 8 * 1000 * 1000 * 1000,
    "tb/s": 8 * 1000 * 1000 * 1000 * 1000,
    "kib/s": 8 * 1024,
    "mib/s": 8 * 1024 * 1024,
    "gib/s": 8 * 1024 * 1024 * 1024,
    "tib/s": 8 * 1024 * 1024 * 1024 * 1024,
    "B/s": 8,
    "KB/s": 8 * 1000,
    "MB/s": 8 * 1000 * 1000,
    "GB/s": 8 * 1000 * 1000 * 1000,
    "TB/s": 8 * 1000 * 1000 * 1000 * 1000,
    "KiB/s": 8 * 1024,
    "MiB/s": 8 * 1024 * 1024,
    "GiB/s": 8 * 1024 * 1024 * 1024,
    "TiB/s": 8 * 1024 * 1024 * 1024 * 1024
})

function _pulseSearchSourceOptions() {
    return {
        "includeApps": _pulseSettingBool("pulse.includeApps", true),
        "includeRecent": _pulseSettingBool("pulse.includeRecent", true),
        "includeClipboard": _pulseSettingBool("pulse.includeClipboard", true),
        "includeTracker": _pulseSettingBool("pulse.includeTracker", true),
        "includeActions": _pulseSettingBool("pulse.includeActions", true),
        "includeSettings": _pulseSettingBool("pulse.includeSettings", true),
        "includePreview": _pulseSettingBool("pulse.enablePreview", true)
    }
}

function _pulseResultLimit() {
    if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
        return 24
    }
    var raw = DesktopSettings.settingValue("pulse.resultLimit", 24)
    var limit = Number(raw)
    if (isNaN(limit)) {
        limit = 24
    }
    limit = Math.floor(limit)
    if (limit < 8) {
        limit = 8
    } else if (limit > 256) {
        limit = 256
    }
    return limit
}

function _pulseAutoFocusOnOpen() {
    return _pulseSettingBool("pulse.autoFocusOnOpen", true)
}

function _pulseEnterOpensFirstResult() {
    return _pulseSettingBool("pulse.enterOpensFirstResult", true)
}

function _pulseAutoCloseAfterLaunch() {
    return _pulseSettingBool("pulse.autoCloseAfterLaunch", true)
}

function _pulseRankBoosts() {
    return {
        "apps": _pulseSettingInt("pulse.rankBoostApps", 0, -40, 80),
        "recent_files": _pulseSettingInt("pulse.rankBoostRecent", 0, -40, 80),
        "clipboard": _pulseSettingInt("pulse.rankBoostClipboard", 0, -40, 80),
        "tracker": _pulseSettingInt("pulse.rankBoostTracker", 0, -40, 80),
        "slm_actions": _pulseSettingInt("pulse.rankBoostActions", 0, -40, 80),
        "settings": _pulseSettingInt("pulse.rankBoostSettings", 0, -40, 80)
    }
}

function _pulseCalculatorNormalizeQuery(text) {
    var t = String(text || "").trim()
    if (t.length <= 0) {
        return ""
    }
    if (t.charAt(0) === "=") {
        t = t.substring(1).trim()
    }
    var lower = t.toLowerCase()
    if (lower.indexOf("calc ") === 0) {
        t = t.substring(5).trim()
    } else if (lower.indexOf("calculate ") === 0) {
        t = t.substring(10).trim()
    } else if (lower.indexOf("convert ") === 0) {
        t = t.substring(8).trim()
    }
    return t
}

function _pulseCalculatorAliasLookup(value, maps) {
    for (var i = 0; i < maps.length; ++i) {
        var map = maps[i]
        if (map && Object.prototype.hasOwnProperty.call(map, value)) {
            return String(map[value] || "")
        }
    }
    return ""
}

var _pulseCalculatorLengthAliases = ({
    "kilometers": "km",
    "kilometer": "km",
    "kilometres": "km",
    "kilometre": "km",
    "nanometers": "nm",
    "nanometer": "nm",
    "nanometres": "nm",
    "nanometre": "nm",
    "meters": "m",
    "meter": "m",
    "metres": "m",
    "metre": "m",
    "centimeters": "cm",
    "centimeter": "cm",
    "centimetres": "cm",
    "centimetre": "cm",
    "millimeters": "mm",
    "millimeter": "mm",
    "millimetres": "mm",
    "millimetre": "mm",
    "inches": "in",
    "inch": "in",
    "feet": "ft",
    "foot": "ft",
    "yards": "yd",
    "yard": "yd",
    "miles": "mi",
    "mile": "mi",
    "nauticalmiles": "nmi",
    "nauticalmile": "nmi"
})

var _pulseCalculatorMassAliases = ({
    "grams": "g",
    "gram": "g",
    "kilograms": "kg",
    "kilogram": "kg",
    "milligrams": "mg",
    "milligram": "mg",
    "ounces": "oz",
    "ounce": "oz",
    "pounds": "lb",
    "pound": "lb",
    "tons": "ton",
    "ton": "ton",
    "tonnes": "t",
    "tonne": "t"
})

var _pulseCalculatorTimeAliases = ({
    "seconds": "s",
    "second": "s",
    "minutes": "min",
    "minute": "min",
    "hours": "h",
    "hour": "h",
    "days": "d",
    "day": "d"
})

var _pulseCalculatorStorageAliases = ({
    "bytes": "b",
    "byte": "b",
    "bits": "bit",
    "bit": "bit",
    "kilobytes": "kb",
    "kilobyte": "kb",
    "megabytes": "mb",
    "megabyte": "mb",
    "gigabytes": "gb",
    "gigabyte": "gb",
    "terabytes": "tb",
    "terabyte": "tb",
    "kibibytes": "kib",
    "kibibyte": "kib",
    "mebibytes": "mib",
    "mebibyte": "mib",
    "gibibytes": "gib",
    "gibibyte": "gib",
    "tebibytes": "tib",
    "tebibyte": "tib"
})

var _pulseCalculatorVolumeAliases = ({
    "liters": "l",
    "liter": "l",
    "litres": "l",
    "litre": "l",
    "milliliters": "ml",
    "milliliter": "ml",
    "millilitres": "ml",
    "millilitre": "ml",
    "centiliters": "cl",
    "centiliter": "cl",
    "centilitres": "cl",
    "centilitre": "cl",
    "deciliters": "dl",
    "deciliter": "dl",
    "decilitres": "dl",
    "decilitre": "dl",
    "gallons": "gal",
    "gallon": "gal",
    "quarts": "qt",
    "quart": "qt",
    "pints": "pt",
    "pint": "pt",
    "cups": "cup",
    "cup": "cup",
    "tablespoons": "tbsp",
    "tablespoon": "tbsp",
    "teaspoons": "tsp",
    "teaspoon": "tsp"
})

var _pulseCalculatorSimpleAliases = ({
    "celsius": "c",
    "fahrenheit": "f",
    "kelvin": "k",
    "rankine": "r",
    "reaumur": "re",
    "newton": "n",
    "newtons": "n",
    "joule": "j",
    "joules": "j",
    "watt": "w",
    "watts": "w",
    "pascals": "pa",
    "pascal": "pa",
    "kilopascal": "kpa",
    "megapascal": "mpa",
    "bar": "bar",
    "atmosphere": "atm",
    "psi": "psi",
    "torr": "torr",
    "mmhg": "mmhg",
    "inhg": "inhg",
    "kilojoule": "kj",
    "megajoule": "mj",
    "watthour": "wh",
    "kilowatthour": "kwh",
    "calorie": "cal",
    "kilocalorie": "kcal",
    "btu": "btu",
    "kilowatt": "kw",
    "milliwatt": "mw",
    "horsepower": "hp",
    "hertz": "hz",
    "kilohertz": "khz",
    "megahertz": "mhz",
    "gigahertz": "ghz",
    "terahertz": "thz",
    "degree": "deg",
    "degrees": "deg",
    "deg": "deg",
    "radian": "rad",
    "radians": "rad",
    "rad": "rad",
    "gradian": "grad",
    "gradians": "grad",
    "gon": "grad",
    "turn": "turn",
    "revolution": "turn",
    "knots": "knot",
    "kt": "knot",
    "mph": "mph",
    "kph": "kph",
    "kmh": "kmh",
    "mps": "mps",
    "fps": "fps",
    "kn": "kn",
    "%": "pct",
    "percent": "pct",
    "ratio": "ratio",
    "fraction": "ratio"
})

function _pulseCalculatorNormalizeUnitToken(unitText) {
    var u = String(unitText || "").trim().toLowerCase()
    u = u.replace(/\s+/g, "")
    u = u.replace(/µ/g, "u")
    u = u.replace(/°/g, "")
    u = u.replace(/\^2/g, "2")
    u = u.replace(/\^3/g, "3")
    u = u.replace(/²/g, "2")
    u = u.replace(/³/g, "3")
    var alias = _pulseCalculatorAliasLookup(u, [
        _pulseCalculatorLengthAliases,
        _pulseCalculatorMassAliases,
        _pulseCalculatorTimeAliases,
        _pulseCalculatorStorageAliases,
        _pulseCalculatorVolumeAliases,
        _pulseCalculatorSimpleAliases
    ])
    if (alias.length > 0) return alias
    if (u === "per") return "/"
    if (u === "m/s2") return "m/s2"
    if (u === "cm/s2") return "cm/s2"
    if (u === "mm/s2") return "mm/s2"
    if (u === "ft/s2") return "ft/s2"
    if (u === "km/h2") return "km/h2"
    if (u === "kg/m3") return "kg/m3"
    if (u === "g/cm3") return "g/cm3"
    if (u === "g/ml") return "g/ml"
    if (u === "kg/l") return "kg/l"
    if (u === "lb/ft3") return "lb/ft3"
    if (u === "m3/s") return "m3/s"
    if (u === "l/s") return "l/s"
    if (u === "ml/s") return "ml/s"
    if (u === "kg/s") return "kg/s"
    if (u === "g/s") return "g/s"
    if (u === "j/s") return "j/s"
    if (u === "n") return "n"
    if (u === "kn") return "kn"
    if (u === "dyn") return "dyn"
    if (u === "lbf") return "lbf"
    if (u === "kgf") return "kgf"
    if (u === "deg/s") return "deg/s"
    if (u === "degree/s") return "deg/s"
    if (u === "degrees/s") return "deg/s"
    if (u === "rad/s") return "rad/s"
    if (u === "radian/s") return "rad/s"
    if (u === "radians/s") return "rad/s"
    if (u === "turn/s") return "turn/s"
    if (u === "rev/s") return "turn/s"
    if (u === "revolution/s") return "turn/s"
    if (u === "rpm") return "rpm"
    if (u === "bps") return "bps"
    if (u === "kbps") return "kbps"
    if (u === "mbps") return "mbps"
    if (u === "gbps") return "gbps"
    if (u === "tbps") return "tbps"
    if (u === "b/s") return "bps"
    if (u === "bit/s") return "bps"
    if (u === "bits/s") return "bps"
    if (u === "byte/s") return "b/s"
    if (u === "bytes/s") return "b/s"
    if (u === "kb/s") return "kb/s"
    if (u === "mb/s") return "mb/s"
    if (u === "gb/s") return "gb/s"
    if (u === "tb/s") return "tb/s"
    if (u === "kib/s") return "kib/s"
    if (u === "mib/s") return "mib/s"
    if (u === "gib/s") return "gib/s"
    if (u === "tib/s") return "tib/s"
    return u
}

function _pulseCalculatorUnitGroup(unit) {
    var u = _pulseCalculatorNormalizeUnitToken(unit)
    if (u === "mm" || u === "cm" || u === "m" || u === "km" || u === "in" || u === "ft" || u === "yd" || u === "mi") {
        return "length"
    }
    if (u === "nmi") {
        return "length"
    }
    if (u === "mm2" || u === "cm2" || u === "m2" || u === "km2" || u === "in2" || u === "ft2" || u === "yd2" || u === "mi2" || u === "nmi2" || u === "acre" || u === "ha") {
        return "area"
    }
    if (u === "mm3" || u === "cm3" || u === "m3" || u === "km3" || u === "in3" || u === "ft3" || u === "yd3" || u === "mi3" || u === "nmi3" || u === "ml" || u === "cl" || u === "dl" || u === "l" || u === "gal" || u === "qt" || u === "pt" || u === "cup" || u === "tbsp" || u === "tsp") {
        return "volume"
    }
    if (u === "mg" || u === "g" || u === "kg" || u === "oz" || u === "lb" || u === "ton" || u === "t") {
        return "mass"
    }
    if (u === "ms" || u === "s" || u === "min" || u === "h" || u === "d") {
        return "time"
    }
    if (u === "b" || u === "bit" || u === "kb" || u === "mb" || u === "gb" || u === "tb" || u === "kib" || u === "mib" || u === "gib" || u === "tib") {
        return "storage"
    }
    if (_pulseCalculatorSpeedUnits[u]) {
        return "speed"
    }
    if (_pulseCalculatorTemperatureUnits[u]) {
        return "temperature"
    }
    if (_pulseCalculatorAngleUnits[u]) {
        return "angle"
    }
    if (_pulseCalculatorAngularSpeedUnits[u]) {
        return "angular_speed"
    }
    if (u === "m/s2" || u === "cm/s2" || u === "mm/s2" || u === "ft/s2" || u === "km/h2") {
        return "acceleration"
    }
    if (u === "kg/m3" || u === "g/cm3" || u === "g/ml" || u === "kg/l" || u === "lb/ft3") {
        return "density"
    }
    if (u === "m3/s" || u === "l/s" || u === "ml/s" || u === "kg/s" || u === "g/s") {
        return "flow"
    }
    if (_pulseCalculatorRatioUnits[u]) {
        return "ratio"
    }
    if (_pulseCalculatorPressureFactors[u] !== undefined) {
        return "pressure"
    }
    if (_pulseCalculatorEnergyFactors[u] !== undefined) {
        return "energy"
    }
    if (_pulseCalculatorPowerFactors[u] !== undefined) {
        return "power"
    }
    if (u === "j/s") {
        return "power"
    }
    if (_pulseCalculatorForceFactors[u] !== undefined) {
        return "force"
    }
    if (_pulseCalculatorFrequencyUnits[u]) {
        return "frequency"
    }
    if (_pulseCalculatorDataRateUnits[u]) {
        return "data_rate"
    }
    return ""
}

function _pulseCalculatorUnitFactor(unit, group) {
    var u = _pulseCalculatorNormalizeUnitToken(unit)
    if (group === "length") {
        if (u === "mm") return 0.001
        if (u === "cm") return 0.01
        if (u === "m") return 1
        if (u === "km") return 1000
        if (u === "in") return 0.0254
        if (u === "ft") return 0.3048
        if (u === "yd") return 0.9144
        if (u === "mi") return 1609.344
        if (u === "nmi") return 1852
    } else if (group === "area") {
        if (u === "mm2") return 0.001 * 0.001
        if (u === "cm2") return 0.01 * 0.01
        if (u === "m2") return 1
        if (u === "km2") return 1000 * 1000
        if (u === "in2") return 0.0254 * 0.0254
        if (u === "ft2") return 0.3048 * 0.3048
        if (u === "yd2") return 0.9144 * 0.9144
        if (u === "mi2") return 1609.344 * 1609.344
        if (u === "nmi2") return 1852 * 1852
        if (u === "acre") return 4046.8564224
        if (u === "ha") return 10000
    } else if (group === "volume") {
        if (u === "mm3") return 0.001 * 0.001 * 0.001
        if (u === "cm3") return 0.01 * 0.01 * 0.01
        if (u === "m3") return 1
        if (u === "km3") return 1000 * 1000 * 1000
        if (u === "in3") return 0.0254 * 0.0254 * 0.0254
        if (u === "ft3") return 0.3048 * 0.3048 * 0.3048
        if (u === "yd3") return 0.9144 * 0.9144 * 0.9144
        if (u === "mi3") return 1609.344 * 1609.344 * 1609.344
        if (u === "nmi3") return 1852 * 1852 * 1852
        if (u === "ml") return 0.000001
        if (u === "cl") return 0.00001
        if (u === "dl") return 0.0001
        if (u === "l") return 0.001
        if (u === "gal") return 0.003785411784
        if (u === "qt") return 0.000946352946
        if (u === "pt") return 0.000473176473
        if (u === "cup") return 0.0002365882365
        if (u === "tbsp") return 0.00001478676478125
        if (u === "tsp") return 0.00000492892159375
    } else if (group === "mass") {
        if (u === "mg") return 0.000001
        if (u === "g") return 0.001
        if (u === "kg") return 1
        if (u === "oz") return 0.028349523125
        if (u === "lb") return 0.45359237
        if (u === "ton") return 907.18474
        if (u === "t") return 1000
    } else if (group === "time") {
        if (u === "ms") return 0.001
        if (u === "s") return 1
        if (u === "min") return 60
        if (u === "h") return 3600
        if (u === "d") return 86400
    } else if (group === "storage") {
        if (u === "b") return 1
        if (u === "bit") return 0.125
        if (u === "kb") return 1000
        if (u === "mb") return 1000 * 1000
        if (u === "gb") return 1000 * 1000 * 1000
        if (u === "tb") return 1000 * 1000 * 1000 * 1000
        if (u === "kib") return 1024
        if (u === "mib") return 1024 * 1024
        if (u === "gib") return 1024 * 1024 * 1024
        if (u === "tib") return 1024 * 1024 * 1024 * 1024
    } else if (group === "speed") {
        var speedFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorSpeedFactors)
        if (speedFactor !== null) return speedFactor
    } else if (group === "temperature") {
        if (u === "k") return 1
        if (u === "c") return 1
        if (u === "f") return 1
        if (u === "r") return 1
        if (u === "re") return 1
    } else if (group === "angle") {
        var angleFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorAngleFactors)
        if (angleFactor !== null) return angleFactor
    } else if (group === "angular_speed") {
        var angularSpeedFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorAngularSpeedFactors)
        if (angularSpeedFactor !== null) return angularSpeedFactor
    } else if (group === "ratio") {
        var ratioFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorRatioFactors)
        if (ratioFactor !== null) return ratioFactor
    } else if (group === "pressure") {
        var pressureFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorPressureFactors)
        if (pressureFactor !== null) return pressureFactor
    } else if (group === "energy") {
        var energyFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorEnergyFactors)
        if (energyFactor !== null) return energyFactor
    } else if (group === "power") {
        var powerFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorPowerFactors)
        if (powerFactor !== null) return powerFactor
    } else if (group === "force") {
        var forceFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorForceFactors)
        if (forceFactor !== null) return forceFactor
    } else if (group === "acceleration") {
        var accelerationFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorAccelerationFactors)
        if (accelerationFactor !== null) return accelerationFactor
    } else if (group === "density") {
        var densityFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorDensityFactors)
        if (densityFactor !== null) return densityFactor
    } else if (group === "flow") {
        var flowFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorFlowFactors)
        if (flowFactor !== null) return flowFactor
    } else if (group === "frequency") {
        var frequencyFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorFrequencyFactors)
        if (frequencyFactor !== null) return frequencyFactor
    } else if (group === "data_rate") {
        var dataRateFactor = _pulseCalculatorFactorLookup(u, _pulseCalculatorDataRateFactors)
        if (dataRateFactor !== null) return dataRateFactor
    }
    return NaN
}

function _pulseCalculatorFormatUnitValue(value, unit) {
    var formatted = _pulseCalculatorFormatNumber(value)
    if (formatted.length <= 0) {
        return ""
    }
    return formatted + " " + String(unit || "")
}

function _pulseCalculatorSplitValueAndUnit(text) {
    var raw = String(text || "").trim()
    if (raw.length <= 0) {
        return null
    }
    var spacedTokens = raw.split(/\s+/)
    if (spacedTokens.length > 1) {
        for (var i = 1; i < spacedTokens.length; ++i) {
            var valueCandidate = spacedTokens.slice(0, spacedTokens.length - i).join(" ")
            var unitCandidate = spacedTokens.slice(spacedTokens.length - i).join(" ")
            if (valueCandidate.length <= 0 || unitCandidate.length <= 0) {
                continue
            }
            var unitLooksValid = false
            if (_pulseCalculatorNormalizeDataRateUnitToken(unitCandidate).length > 0) {
                unitLooksValid = true
            } else if (_pulseCalculatorUnitGroup(unitCandidate).length > 0) {
                unitLooksValid = true
            } else {
                var compoundSpec = _pulseCalculatorParseUnitExpression(unitCandidate)
                unitLooksValid = !!(compoundSpec && compoundSpec.ok)
            }
            if (!unitLooksValid) {
                continue
            }
            var valueCheck = _pulseCalculatorEvaluate(valueCandidate)
            if (valueCheck && valueCheck.ok) {
                return {
                    "valueExpr": valueCandidate,
                    "unit": unitCandidate
                }
            }
        }
    }
    var spaced = raw.match(/^(.+?)\s+([a-zA-Z0-9µ°\/\^²³\.\-%*×·]+)$/i)
    if (spaced) {
        return {
            "valueExpr": String(spaced[1] || "").trim(),
            "unit": String(spaced[2] || "").trim()
        }
    }
    var attached = raw.match(/^(.+?)([a-zA-Zµ°\/\^²³%*×·]+)$/i)
    if (attached) {
        return {
            "valueExpr": String(attached[1] || "").trim(),
            "unit": String(attached[2] || "").trim()
        }
    }
    return null
}

function _pulseCalculatorUnitDimKey(dims) {
    var keys = []
    for (var k in dims) {
        var v = Number(dims[k] || 0)
        if (v !== 0) {
            keys.push(k + ":" + String(v))
        }
    }
    keys.sort()
    return keys.join("|")
}

function _pulseCalculatorUnitDimAdd(target, source, multiplier) {
    var m = Number(multiplier || 0)
    if (!isFinite(m) || m === 0) {
        return target
    }
    for (var key in source) {
        var value = Number(source[key] || 0)
        if (value === 0) {
            continue
        }
        target[key] = Number(target[key] || 0) + value * m
        if (Math.abs(target[key]) < 1e-12) {
            delete target[key]
        }
    }
    return target
}

var _pulseCalculatorCompoundLabelDirectGroups = ({
    "force": "Force",
    "power": "Power",
    "pressure": "Pressure",
    "energy": "Energy"
})

var _pulseCalculatorCompoundLabelRules = [
    {
        "dimKey": "L:2|M:1|T:-2",
        "label": "Torque",
        "patterns": [
            "\\bnewton\\s*m\\b",
            "\\bn\\s*\\*\\s*m\\b",
            "\\bn\\s+m\\b",
            "\\blbf\\s*\\*\\s*ft\\b",
            "\\blbf\\s*ft\\b",
            "\\bkgf\\s*\\*\\s*m\\b",
            "\\bkgf\\s*m\\b",
            "\\bnewton\\s*meter\\b"
        ]
    },
    {
        "dimKey": "L:2|M:1|T:-2",
        "label": "Energy",
        "patterns": [
            "\\bjoule\\b",
            "\\bj\\b",
            "\\bkilojoule\\b",
            "\\bkj\\b",
            "\\bmegajoule\\b",
            "\\bmj\\b",
            "\\bwatt\\s*hour\\b",
            "\\bwatthour\\b",
            "\\bkilowatt\\s*hour\\b",
            "\\bkilowatthour\\b",
            "\\bwh\\b",
            "\\bkwh\\b",
            "\\bcalorie\\b",
            "\\bcal\\b",
            "\\bkilocalorie\\b",
            "\\bkcal\\b",
            "\\bbtu\\b"
        ]
    },
    {
        "dimKey": "L:1|M:1|T:-1",
        "label": "Momentum",
        "patterns": [
            "\\bnewton\\s*second\\b",
            "\\bn\\s*\\*\\s*s\\b",
            "\\bn\\s*s\\b",
            "\\bkg\\s*\\*\\s*m\\s*\\/\\s*s\\b",
            "\\bkg\\s*m\\s*\\/\\s*s\\b",
            "\\blbf\\s*\\*\\s*s\\b",
            "\\blbf\\s*s\\b",
            "\\bkilogram\\s+meter\\s+per\\s+second\\b"
        ]
    },
    {
        "dimKey": "L:1|M:1|T:-1",
        "label": "Force",
        "patterns": [
            "\\bnewton\\b",
            "\\bn\\b",
            "\\blbf\\b",
            "\\bkgf\\b",
            "\\bdyn\\b"
        ]
    },
    {
        "dimKey": "L:1|M:1|T:-2",
        "label": "Force",
        "patterns": [
            "\\bnewton\\b",
            "\\bn\\b",
            "\\blbf\\b",
            "\\bkgf\\b",
            "\\bdyn\\b"
        ]
    },
    {
        "dimKey": "L:2|M:1|T:-3",
        "label": "Power",
        "patterns": [
            "\\bjoule\\s*per\\s*second\\b",
            "\\bj\\s*\\/\\s*s\\b",
            "\\bwatt\\b",
            "\\bw\\b",
            "\\bkilowatt\\s*hour\\b",
            "\\bwatt\\s*hour\\b",
            "\\bkilowatthour\\b",
            "\\bwatthour\\b"
        ]
    },
    {
        "dimKey": "L:-1|M:1|T:-2",
        "label": "Pressure",
        "patterns": [
            "\\bpascal\\b",
            "\\bpa\\b",
            "\\bkpa\\b",
            "\\bmpa\\b",
            "\\bbar\\b",
            "\\batm\\b",
            "\\bpsi\\b",
            "\\btorr\\b",
            "\\bmmhg\\b",
            "\\binhg\\b"
        ]
    }
]

function _pulseCalculatorCompoundLabelMatches(text, patterns) {
    var haystack = String(text || "")
    for (var i = 0; i < patterns.length; ++i) {
        if (new RegExp(patterns[i], "i").test(haystack)) {
            return true
        }
    }
    return false
}

function _pulseCalculatorCompoundLabel(sourceSpec, targetSpec, sourceUnitRaw, targetUnitRaw, sourceGroup, targetGroup) {
    var sourceDimKey = String((sourceSpec && sourceSpec.dimKey) || "")
    var targetDimKey = String((targetSpec && targetSpec.dimKey) || "")
    var dimKey = sourceDimKey.length > 0 ? sourceDimKey : targetDimKey
    var combinedUnits = (String(sourceUnitRaw || "").toLowerCase() + " " + String(targetUnitRaw || "").toLowerCase())

    if (sourceGroup.length > 0 && sourceGroup === targetGroup && _pulseCalculatorCompoundLabelDirectGroups[sourceGroup]) {
        return _pulseCalculatorCompoundLabelDirectGroups[sourceGroup]
    }

    for (var r = 0; r < _pulseCalculatorCompoundLabelRules.length; ++r) {
        var rule = _pulseCalculatorCompoundLabelRules[r]
        if (rule.dimKey !== dimKey) {
            continue
        }
        if (_pulseCalculatorCompoundLabelMatches(combinedUnits, rule.patterns || [])) {
            return rule.label
        }
    }
    return ""
}

function _pulseCalculatorUnitSpec(unitText) {
    var u = _pulseCalculatorNormalizeUnitToken(unitText)
    if (u.length <= 0) {
        return null
    }
    var one = function(factor, dims) {
        return { "factor": factor, "dims": dims }
    }
    if (u === "m") return one(1, { "L": 1 })
    if (u === "nm") return one(0.000000001, { "L": 1 })
    if (u === "cm") return one(0.01, { "L": 1 })
    if (u === "mm") return one(0.001, { "L": 1 })
    if (u === "km") return one(1000, { "L": 1 })
    if (u === "in") return one(0.0254, { "L": 1 })
    if (u === "ft") return one(0.3048, { "L": 1 })
    if (u === "yd") return one(0.9144, { "L": 1 })
    if (u === "mi") return one(1609.344, { "L": 1 })
    if (u === "nmi") return one(1852, { "L": 1 })
    if (u === "m2") return one(1, { "L": 2 })
    if (u === "cm2") return one(0.01 * 0.01, { "L": 2 })
    if (u === "mm2") return one(0.001 * 0.001, { "L": 2 })
    if (u === "km2") return one(1000 * 1000, { "L": 2 })
    if (u === "in2") return one(0.0254 * 0.0254, { "L": 2 })
    if (u === "ft2") return one(0.3048 * 0.3048, { "L": 2 })
    if (u === "yd2") return one(0.9144 * 0.9144, { "L": 2 })
    if (u === "mi2") return one(1609.344 * 1609.344, { "L": 2 })
    if (u === "nmi2") return one(1852 * 1852, { "L": 2 })
    if (u === "acre") return one(4046.8564224, { "L": 2 })
    if (u === "ha") return one(10000, { "L": 2 })
    if (u === "m3") return one(1, { "L": 3 })
    if (u === "cm3") return one(0.01 * 0.01 * 0.01, { "L": 3 })
    if (u === "mm3") return one(0.001 * 0.001 * 0.001, { "L": 3 })
    if (u === "km3") return one(1000 * 1000 * 1000, { "L": 3 })
    if (u === "in3") return one(0.0254 * 0.0254 * 0.0254, { "L": 3 })
    if (u === "ft3") return one(0.3048 * 0.3048 * 0.3048, { "L": 3 })
    if (u === "yd3") return one(0.9144 * 0.9144 * 0.9144, { "L": 3 })
    if (u === "mi3") return one(1609.344 * 1609.344 * 1609.344, { "L": 3 })
    if (u === "nmi3") return one(1852 * 1852 * 1852, { "L": 3 })
    if (u === "l") return one(0.001, { "L": 3 })
    if (u === "cl") return one(0.00001, { "L": 3 })
    if (u === "dl") return one(0.0001, { "L": 3 })
    if (u === "ml") return one(0.000001, { "L": 3 })
    if (u === "gal") return one(0.003785411784, { "L": 3 })
    if (u === "qt") return one(0.000946352946, { "L": 3 })
    if (u === "pt") return one(0.000473176473, { "L": 3 })
    if (u === "cup") return one(0.0002365882365, { "L": 3 })
    if (u === "tbsp") return one(0.00001478676478125, { "L": 3 })
    if (u === "tsp") return one(0.00000492892159375, { "L": 3 })
    if (u === "g") return one(0.001, { "M": 1 })
    if (u === "kg") return one(1, { "M": 1 })
    if (u === "mg") return one(0.000001, { "M": 1 })
    if (u === "oz") return one(0.028349523125, { "M": 1 })
    if (u === "lb") return one(0.45359237, { "M": 1 })
    if (u === "ton") return one(907.18474, { "M": 1 })
    if (u === "t") return one(1000, { "M": 1 })
    if (u === "s") return one(1, { "T": 1 })
    if (u === "ms") return one(0.001, { "T": 1 })
    if (u === "min") return one(60, { "T": 1 })
    if (u === "h") return one(3600, { "T": 1 })
    if (u === "d") return one(86400, { "T": 1 })
    if (u === "rad") return one(1, { "A": 1 })
    if (u === "deg") return one(Math.PI / 180, { "A": 1 })
    if (u === "grad") return one(Math.PI / 200, { "A": 1 })
    if (u === "turn") return one(Math.PI * 2, { "A": 1 })
    if (u === "pct") return one(0.01, {})
    if (u === "ratio") return one(1, {})
    if (u === "hz") return one(1, { "T": -1 })
    if (u === "khz") return one(1000, { "T": -1 })
    if (u === "mhz") return one(1000 * 1000, { "T": -1 })
    if (u === "ghz") return one(1000 * 1000 * 1000, { "T": -1 })
    if (u === "thz") return one(1000 * 1000 * 1000 * 1000, { "T": -1 })
    if (u === "rpm") return one(1 / 60, { "T": -1 })
    if (u === "n") return one(1, { "M": 1, "L": 1, "T": -2 })
    if (u === "kn") return one(1000, { "M": 1, "L": 1, "T": -2 })
    if (u === "dyn") return one(0.00001, { "M": 1, "L": 1, "T": -2 })
    if (u === "lbf") return one(4.4482216152605, { "M": 1, "L": 1, "T": -2 })
    if (u === "kgf") return one(9.80665, { "M": 1, "L": 1, "T": -2 })
    if (u === "pa") return one(1, { "M": 1, "L": -1, "T": -2 })
    if (u === "kpa") return one(1000, { "M": 1, "L": -1, "T": -2 })
    if (u === "mpa") return one(1000 * 1000, { "M": 1, "L": -1, "T": -2 })
    if (u === "bar") return one(100000, { "M": 1, "L": -1, "T": -2 })
    if (u === "atm") return one(101325, { "M": 1, "L": -1, "T": -2 })
    if (u === "psi") return one(6894.757293168, { "M": 1, "L": -1, "T": -2 })
    if (u === "torr") return one(133.32236842105263, { "M": 1, "L": -1, "T": -2 })
    if (u === "mmhg") return one(133.32236842105263, { "M": 1, "L": -1, "T": -2 })
    if (u === "inhg") return one(3386.389, { "M": 1, "L": -1, "T": -2 })
    if (u === "j") return one(1, { "M": 1, "L": 2, "T": -2 })
    if (u === "kj") return one(1000, { "M": 1, "L": 2, "T": -2 })
    if (u === "mj") return one(1000 * 1000, { "M": 1, "L": 2, "T": -2 })
    if (u === "wh") return one(3600, { "M": 1, "L": 2, "T": -2 })
    if (u === "kwh") return one(3600 * 1000, { "M": 1, "L": 2, "T": -2 })
    if (u === "cal") return one(4.184, { "M": 1, "L": 2, "T": -2 })
    if (u === "kcal") return one(4184, { "M": 1, "L": 2, "T": -2 })
    if (u === "btu") return one(1055.05585262, { "M": 1, "L": 2, "T": -2 })
    if (u === "w") return one(1, { "M": 1, "L": 2, "T": -3 })
    if (u === "kw") return one(1000, { "M": 1, "L": 2, "T": -3 })
    if (u === "mw") return one(0.001, { "M": 1, "L": 2, "T": -3 })
    if (u === "hp") return one(745.6998715822702, { "M": 1, "L": 2, "T": -3 })
    if (u === "m/s") return one(1, { "L": 1, "T": -1 })
    if (u === "km/h") return one(1000 / 3600, { "L": 1, "T": -1 })
    if (u === "mph") return one(1609.344 / 3600, { "L": 1, "T": -1 })
    if (u === "kph") return one(1000 / 3600, { "L": 1, "T": -1 })
    if (u === "kmh") return one(1000 / 3600, { "L": 1, "T": -1 })
    if (u === "mps") return one(1, { "L": 1, "T": -1 })
    if (u === "fps") return one(0.3048, { "L": 1, "T": -1 })
    if (u === "knot") return one(1852 / 3600, { "L": 1, "T": -1 })
    if (u === "deg/s") return one(Math.PI / 180, { "A": 1, "T": -1 })
    if (u === "rad/s") return one(1, { "A": 1, "T": -1 })
    if (u === "turn/s") return one(Math.PI * 2, { "A": 1, "T": -1 })
    if (u === "m/s2") return one(1, { "L": 1, "T": -2 })
    if (u === "cm/s2") return one(0.01, { "L": 1, "T": -2 })
    if (u === "mm/s2") return one(0.001, { "L": 1, "T": -2 })
    if (u === "ft/s2") return one(0.3048, { "L": 1, "T": -2 })
    if (u === "km/h2") return one(1000 / (3600 * 3600), { "L": 1, "T": -2 })
    if (u === "kg/m3") return one(1, { "M": 1, "L": -3 })
    if (u === "g/cm3") return one(1000, { "M": 1, "L": -3 })
    if (u === "g/ml") return one(1000, { "M": 1, "L": -3 })
    if (u === "kg/l") return one(1000, { "M": 1, "L": -3 })
    if (u === "lb/ft3") return one(16.01846337396014, { "M": 1, "L": -3 })
    if (u === "m3/s") return one(1, { "L": 3, "T": -1 })
    if (u === "l/s") return one(0.001, { "L": 3, "T": -1 })
    if (u === "ml/s") return one(0.000001, { "L": 3, "T": -1 })
    if (u === "kg/s") return one(1, { "M": 1, "T": -1 })
    if (u === "g/s") return one(0.001, { "M": 1, "T": -1 })
    if (u === "bps") return one(1, { "I": 1, "T": -1 })
    if (u === "kbps") return one(1000, { "I": 1, "T": -1 })
    if (u === "mbps") return one(1000 * 1000, { "I": 1, "T": -1 })
    if (u === "gbps") return one(1000 * 1000 * 1000, { "I": 1, "T": -1 })
    if (u === "tbps") return one(1000 * 1000 * 1000 * 1000, { "I": 1, "T": -1 })
    if (u === "b/s") return one(1, { "I": 1, "T": -1 })
    if (u === "kb/s") return one(8 * 1000, { "I": 1, "T": -1 })
    if (u === "mb/s") return one(8 * 1000 * 1000, { "I": 1, "T": -1 })
    if (u === "gb/s") return one(8 * 1000 * 1000 * 1000, { "I": 1, "T": -1 })
    if (u === "tb/s") return one(8 * 1000 * 1000 * 1000 * 1000, { "I": 1, "T": -1 })
    if (u === "kib/s") return one(8 * 1024, { "I": 1, "T": -1 })
    if (u === "mib/s") return one(8 * 1024 * 1024, { "I": 1, "T": -1 })
    if (u === "gib/s") return one(8 * 1024 * 1024 * 1024, { "I": 1, "T": -1 })
    if (u === "tib/s") return one(8 * 1024 * 1024 * 1024 * 1024, { "I": 1, "T": -1 })
    return null
}

function _pulseCalculatorParseUnitExpression(unitText) {
    var raw = String(unitText || "").trim()
    if (raw.length <= 0) {
        return null
    }
    var expr = raw.replace(/\bkilograms?\s+meters?\s+per\s+seconds?\s+squared\b/gi, "kg*m/s2")
                  .replace(/\bkilograms?\s+meters?\s+per\s+seconds?\b/gi, "kg*m/s")
                  .replace(/\bnewtons?\s+meters?\b/gi, "n*m")
                  .replace(/\bjoules?\s+per\s+seconds?\b/gi, "j/s")
                  .replace(/\bnewtons?\s+seconds?\b/gi, "n*s")
                  .replace(/\bjoules?\s+meters?\b/gi, "j*m")
                  .replace(/×|·/g, "*")
                  .replace(/\bper\b/g, "/")
                  .replace(/\^([23])/g, "$1")
                  .replace(/²/g, "2")
                  .replace(/³/g, "3")
    if (expr.length <= 0) {
        return null
    }
    if (expr.indexOf("to") >= 0 || expr.indexOf("in") >= 0) {
        return null
    }
    var tokens = expr.match(/([*/])|([^\s*/]+)/g)
    if (!tokens || tokens.length <= 0) {
        return null
    }
    var dims = {}
    var factor = 1
    var op = 1
    for (var i = 0; i < tokens.length; ++i) {
        var token = String(tokens[i] || "")
        if (token === "*") {
            op = 1
            continue
        }
        if (token === "/") {
            op = -1
            continue
        }
        var unitMatch = token.match(/^(.+?)([23])?$/)
        if (!unitMatch) {
            return null
        }
        var unitBase = String(unitMatch[1] || "").trim()
        var exponent = Number(unitMatch[2] || 1)
        if (!isFinite(exponent) || exponent <= 0) {
            exponent = 1
        }
        exponent *= op
        var spec = _pulseCalculatorUnitSpec(unitBase)
        if (!spec) {
            return null
        }
        factor *= Math.pow(Number(spec.factor || 1), exponent)
        _pulseCalculatorUnitDimAdd(dims, spec.dims || {}, exponent)
        op = 1
    }
    return {
        "ok": true,
        "factor": factor,
        "dims": dims,
        "dimKey": _pulseCalculatorUnitDimKey(dims)
    }
}

function _pulseCalculatorEvaluateCompoundUnitConversion(text) {
    var raw = _pulseCalculatorNormalizeQuery(text)
    if (raw.length <= 0 || raw.length > 128) {
        return null
    }
    var match = raw.match(/^(.+?)\s+(?:to|in)\s+([a-zA-Z0-9µ°\/\^²³\.\-*×·]+)$/i)
    if (!match) {
        return null
    }
    var left = String(match[1] || "").trim()
    var targetUnitRaw = String(match[2] || "").trim()
    var sourceHasSeparator = /[\s*/×·]/.test(left) || /\bper\b/i.test(left)
    var targetHasSeparator = /[\s*/×·]/.test(targetUnitRaw) || /\bper\b/i.test(targetUnitRaw)
    if (!sourceHasSeparator && !targetHasSeparator) {
        return null
    }
    var leftParts = _pulseCalculatorSplitValueAndUnit(left)
    if (!leftParts) {
        return null
    }
    var valueExpr = String(leftParts.valueExpr || "").trim()
    var sourceUnitRaw = String(leftParts.unit || "").trim()
    var sourceSpec = _pulseCalculatorParseUnitExpression(sourceUnitRaw)
    var targetSpec = _pulseCalculatorParseUnitExpression(targetUnitRaw)
    if (!sourceSpec || !targetSpec) {
        return null
    }
    if (sourceSpec.dimKey !== targetSpec.dimKey) {
        return null
    }
    var compoundLabel = _pulseCalculatorCompoundLabel(sourceSpec, targetSpec, sourceUnitRaw, targetUnitRaw, "", "")
    var numericValue = _pulseCalculatorEvaluate(valueExpr)
    if (!numericValue || !numericValue.ok) {
        return null
    }
    var resultValue = Number(numericValue.value) * Number(sourceSpec.factor || 1) / Number(targetSpec.factor || 1)
    if (!isFinite(resultValue)) {
        return null
    }
    _pulseCalculatorLastValue = resultValue
    return {
        "ok": true,
        "expression": raw,
        "kind": "conversion",
        "inputValue": Number(numericValue.value),
        "sourceUnit": sourceUnitRaw,
        "targetUnit": targetUnitRaw,
        "compoundLabel": compoundLabel,
        "formatted": _pulseCalculatorFormatUnitValue(resultValue, targetUnitRaw),
        "value": resultValue
    }
}

function _pulseCalculatorNormalizeDataRateUnitToken(unitText) {
    var raw = String(unitText || "").trim().replace(/\s+/g, "")
    if (raw.length <= 0) {
        return ""
    }
    var match = raw.match(/^([kmgt]?i?)([bB])(?:ps|\/s)$/i)
    if (!match) {
        return ""
    }
    var prefix = String(match[1] || "").toLowerCase()
    var byteMarker = String(match[2] || "")
    var prefixTable = ({
        "": true,
        "k": true,
        "ki": true,
        "m": true,
        "mi": true,
        "g": true,
        "gi": true,
        "t": true,
        "ti": true
    })
    if (!prefixTable[prefix]) {
        return ""
    }
    var isBytes = byteMarker === "B"
    if (isBytes) {
        if (prefix.length <= 0) {
            return "B/s"
        }
        return prefix.toUpperCase() + "B/s"
    }
    if (prefix.length <= 0) {
        return "bps"
    }
    return prefix + "bps"
}

function _pulseCalculatorEvaluateUnitConversion(text) {
    var raw = _pulseCalculatorNormalizeQuery(text)
    if (raw.length <= 0 || raw.length > 128) {
        return null
    }
    var match = raw.match(/^(.+?)\s+(?:to|in)\s+([a-zA-Z0-9µ°\/\^²³\.\-%*×·\s]+)$/i)
    if (!match) {
        return null
    }
    var left = String(match[1] || "").trim()
    var targetUnitRaw = String(match[2] || "").trim()
    var leftParts = _pulseCalculatorSplitValueAndUnit(left)
    if (!leftParts) {
        return null
    }
    var valueExpr = String(leftParts.valueExpr || "").trim()
    var sourceUnitRaw = String(leftParts.unit || "").trim()
    var sourceDataRateUnit = _pulseCalculatorNormalizeDataRateUnitToken(sourceUnitRaw)
    var targetDataRateUnit = _pulseCalculatorNormalizeDataRateUnitToken(targetUnitRaw)
    var sourceUnit = sourceDataRateUnit.length > 0 ? sourceDataRateUnit : _pulseCalculatorNormalizeUnitToken(sourceUnitRaw)
    var targetUnit = targetDataRateUnit.length > 0 ? targetDataRateUnit : _pulseCalculatorNormalizeUnitToken(targetUnitRaw)
    var group = sourceDataRateUnit.length > 0 ? "data_rate" : _pulseCalculatorUnitGroup(sourceUnit)
    if (group.length <= 0 || group !== (targetDataRateUnit.length > 0 ? "data_rate" : _pulseCalculatorUnitGroup(targetUnit))) {
        var compoundResult = _pulseCalculatorEvaluateCompoundUnitConversion(raw)
        if (compoundResult && compoundResult.ok) {
            return compoundResult
        }
        return null
    }
    var numericValue = _pulseCalculatorEvaluate(valueExpr)
    if (!numericValue || !numericValue.ok) {
        return null
    }
    var baseValue = Number(numericValue.value)
    var resultValue = NaN
    if (group === "temperature") {
        if (sourceUnit === "c") baseValue = baseValue + 273.15
        else if (sourceUnit === "f") baseValue = (baseValue - 32) * 5 / 9 + 273.15
        else if (sourceUnit === "k") baseValue = baseValue
        else if (sourceUnit === "r") baseValue = baseValue * 5 / 9
        else if (sourceUnit === "re") baseValue = baseValue * 1.25 + 273.15
        if (targetUnit === "c") resultValue = baseValue - 273.15
        else if (targetUnit === "f") resultValue = (baseValue - 273.15) * 9 / 5 + 32
        else if (targetUnit === "k") resultValue = baseValue
        else if (targetUnit === "r") resultValue = baseValue * 9 / 5
        else if (targetUnit === "re") resultValue = (baseValue - 273.15) * 0.8
    } else {
        var sourceFactor = _pulseCalculatorUnitFactor(sourceUnit, group)
        var targetFactor = _pulseCalculatorUnitFactor(targetUnit, group)
        if (!isFinite(sourceFactor) || !isFinite(targetFactor) || sourceFactor <= 0 || targetFactor <= 0) {
            var compoundInvalid = _pulseCalculatorEvaluateCompoundUnitConversion(raw)
            if (compoundInvalid && compoundInvalid.ok) {
                return compoundInvalid
            }
            return null
        }
        var baseCanonical = baseValue * sourceFactor
        resultValue = baseCanonical / targetFactor
    }
    if (!isFinite(resultValue)) {
        var compoundFallback = _pulseCalculatorEvaluateCompoundUnitConversion(raw)
        if (compoundFallback && compoundFallback.ok) {
            return compoundFallback
        }
        return null
    }
    _pulseCalculatorLastValue = resultValue
    var sourceSpec = null
    var targetSpec = null
    if (group !== "temperature") {
        sourceSpec = _pulseCalculatorUnitSpec(sourceUnitRaw)
        targetSpec = _pulseCalculatorUnitSpec(targetUnitRaw)
    }
    var compoundLabel = _pulseCalculatorCompoundLabel(sourceSpec, targetSpec, sourceUnitRaw, targetUnitRaw, group, group)
    return {
        "ok": true,
        "expression": raw,
        "kind": "conversion",
        "inputValue": baseValue,
        "sourceUnit": sourceUnit,
        "targetUnit": targetUnit,
        "compoundLabel": compoundLabel,
        "formatted": _pulseCalculatorFormatUnitValue(resultValue, targetUnitRaw || targetUnit),
        "value": resultValue
    }
}

function _pulseCalculatorLooksLikeExpression(text) {
    var expr = _pulseCalculatorNormalizeQuery(text)
    if (expr.length <= 0 || expr.length > 128) {
        return false
    }
    if (!(/[0-9]/.test(expr))) {
        return false
    }
    if (!(/[+\-*/%^()]/.test(expr) || /\b(?:sqrt|sin|cos|tan|asin|acos|atan|log|ln|abs|round|ceil|floor|pi|tau|e)\b/i.test(expr))) {
        return false
    }
    if (!/^[0-9A-Za-z_+\-*/%^().,\s×÷π]+$/.test(expr)) {
        return false
    }
    return true
}

function _pulseCalculatorFormatNumber(value) {
    var n = Number(value)
    if (!isFinite(n)) {
        return ""
    }
    if (Math.abs(n) < 1e-12) {
        n = 0
    }
    if (Number.isInteger(n)) {
        return String(n)
    }
    var raw = n.toPrecision(12)
    if (raw.indexOf("e") >= 0 || raw.indexOf("E") >= 0) {
        return raw
    }
    raw = raw.replace(/\.?0+$/, "")
    return raw.length > 0 ? raw : String(n)
}

function _pulseCalculatorEvaluate(text) {
    var expr = _pulseCalculatorNormalizeQuery(text)
    var conversionResult = _pulseCalculatorEvaluateUnitConversion(expr)
    if (conversionResult && conversionResult.ok) {
        return conversionResult
    }
    if (/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(expr)) {
        var numericLiteral = Number(expr)
        if (isFinite(numericLiteral)) {
            _pulseCalculatorLastValue = numericLiteral
            return {
                "ok": true,
                "expression": expr,
                "kind": "expression",
                "value": numericLiteral,
                "formatted": _pulseCalculatorFormatNumber(numericLiteral)
            }
        }
    }
    if (!_pulseCalculatorLooksLikeExpression(expr)) {
        return null
    }
    expr = expr.replace(/×/g, "*")
               .replace(/÷/g, "/")
               .replace(/−/g, "-")
               .replace(/π/g, "pi")
               .replace(/\bans\b/gi, String(_pulseCalculatorLastValue))

    var tokens = []
    var i = 0
    while (i < expr.length) {
        var ch = expr.charAt(i)
        if (/\s/.test(ch)) {
            ++i
            continue
        }
        if ((ch >= "0" && ch <= "9") || ch === ".") {
            var start = i
            var seenDot = ch === "."
            ++i
            while (i < expr.length) {
                var c = expr.charAt(i)
                if (c >= "0" && c <= "9") {
                    ++i
                    continue
                }
                if (c === "." && !seenDot) {
                    seenDot = true
                    ++i
                    continue
                }
                if ((c === "e" || c === "E") && i + 1 < expr.length) {
                    var expSign = expr.charAt(i + 1)
                    if (expSign === "+" || expSign === "-" || (expSign >= "0" && expSign <= "9")) {
                        i += 2
                        while (i < expr.length && expr.charAt(i) >= "0" && expr.charAt(i) <= "9") {
                            ++i
                        }
                        continue
                    }
                }
                break
            }
            tokens.push({ "type": "number", "value": Number(expr.slice(start, i)) })
            continue
        }
        if ((ch >= "A" && ch <= "Z") || (ch >= "a" && ch <= "z") || ch === "_") {
            var s = i
            ++i
            while (i < expr.length) {
                var cc = expr.charAt(i)
                if ((cc >= "A" && cc <= "Z") || (cc >= "a" && cc <= "z") || (cc >= "0" && cc <= "9") || cc === "_") {
                    ++i
                } else {
                    break
                }
            }
            tokens.push({ "type": "ident", "value": expr.slice(s, i).toLowerCase() })
            continue
        }
        if ("+-*/%^(),".indexOf(ch) >= 0) {
            tokens.push({ "type": ch, "value": ch })
            ++i
            continue
        }
        return null
    }

    var pos = 0
    function peek() {
        return pos < tokens.length ? tokens[pos] : null
    }
    function next() {
        return pos < tokens.length ? tokens[pos++] : null
    }
    function expect(typeValue) {
        var tk = next()
        return !!tk && tk.type === typeValue
    }

    function parseExpression() {
        var value = parseTerm()
        while (true) {
            var tk = peek()
            if (!tk || (tk.type !== "+" && tk.type !== "-")) {
                break
            }
            next()
            var rhs = parseTerm()
            if (tk.type === "+") {
                value += rhs
            } else {
                value -= rhs
            }
        }
        return value
    }

    function parseTerm() {
        var value = parsePower()
        while (true) {
            var tk = peek()
            if (!tk || (tk.type !== "*" && tk.type !== "/" && tk.type !== "%")) {
                break
            }
            next()
            var rhs = parsePower()
            if (tk.type === "*") {
                value *= rhs
            } else if (tk.type === "/") {
                value /= rhs
            } else {
                value %= rhs
            }
        }
        return value
    }

    function parsePower() {
        var base = parseUnary()
        var tk = peek()
        if (tk && tk.type === "^") {
            next()
            var exponent = parsePower()
            base = Math.pow(base, exponent)
        }
        return base
    }

    function parseUnary() {
        var tk = peek()
        if (tk && (tk.type === "+" || tk.type === "-")) {
            next()
            var value = parseUnary()
            return tk.type === "-" ? -value : value
        }
        return parsePrimary()
    }

    function parsePrimary() {
        var tk = peek()
        if (!tk) {
            throw new Error("Unexpected end of expression")
        }
        if (tk.type === "number") {
            next()
            var value = Number(tk.value)
            while (peek() && peek().type === "%") {
                next()
                value = value / 100.0
            }
            return value
        }
        if (tk.type === "ident") {
            next()
            var ident = String(tk.value || "").toLowerCase()
            if (ident === "pi") return Math.PI
            if (ident === "e") return Math.E
            if (ident === "tau") return Math.PI * 2
            if (peek() && peek().type === "(") {
                next()
                var arg = parseExpression()
                if (!expect(")")) {
                    throw new Error("Missing closing parenthesis")
                }
                var fnMap = {
                    "sqrt": Math.sqrt,
                    "abs": Math.abs,
                    "ceil": Math.ceil,
                    "floor": Math.floor,
                    "round": Math.round,
                    "sin": Math.sin,
                    "cos": Math.cos,
                    "tan": Math.tan,
                    "asin": Math.asin,
                    "acos": Math.acos,
                    "atan": Math.atan,
                    "log": Math.log10 ? Math.log10 : function(v) { return Math.log(v) / Math.LN10 },
                    "ln": Math.log,
                    "exp": Math.exp
                }
                var fn = fnMap[ident]
                if (!fn) {
                    throw new Error("Unsupported function")
                }
                return fn(arg)
            }
            throw new Error("Unknown identifier")
        }
        if (tk.type === "(") {
            next()
            var inner = parseExpression()
            if (!expect(")")) {
                throw new Error("Missing closing parenthesis")
            }
            while (peek() && peek().type === "%") {
                next()
                inner = inner / 100.0
            }
            return inner
        }
        throw new Error("Unexpected token")
    }

    try {
        var value = parseExpression()
        if (pos !== tokens.length) {
            return null
        }
        if (!isFinite(value)) {
            return null
        }
        var formatted = _pulseCalculatorFormatNumber(value)
        if (formatted.length <= 0) {
            return null
        }
        _pulseCalculatorLastValue = value
        return {
            "ok": true,
            "expression": expr,
            "kind": "expression",
            "value": value,
            "formatted": formatted
        }
    } catch (err) {
        return null
    }
}

function _globalMenuSearchRows(queryText, limit) {
    if (typeof GlobalMenuManager === "undefined" || !GlobalMenuManager) {
        return []
    }
    if (!GlobalMenuManager.available || !GlobalMenuManager.topLevelMenus || !GlobalMenuManager.menuItemsFor) {
        return []
    }
    var menus = GlobalMenuManager.topLevelMenus
    if (!menus || menus.length <= 0) {
        return []
    }
    var q = String(queryText || "").trim().toLowerCase()
    var maxRows = Number(limit || 24)
    if (maxRows <= 0) {
        maxRows = 24
    }
    var rows = []
    var activeAppId = String(GlobalMenuManager.activeAppId || "").trim()
    for (var mi = 0; mi < menus.length; ++mi) {
        if (rows.length >= maxRows) {
            break
        }
        var menu = menus[mi] || ({})
        var menuId = Number(menu.id || -1)
        var menuLabel = String(menu.label || "").trim()
        if (menuId <= 0 || menuLabel.length <= 0) {
            continue
        }
        var items = GlobalMenuManager.menuItemsFor(menuId)
        if (!items || items.length <= 0) {
            continue
        }
        for (var ii = 0; ii < items.length; ++ii) {
            if (rows.length >= maxRows) {
                break
            }
            var item = items[ii] || ({})
            var itemId = Number(item.id || -1)
            var itemLabel = String(item.label || "").trim()
            if (itemId <= 0 || itemLabel.length <= 0 || itemLabel === "-") {
                continue
            }
            var haystack = (menuLabel + " " + itemLabel + " " + activeAppId).toLowerCase()
            if (q.length > 0 && haystack.indexOf(q) < 0) {
                continue
            }
            rows.push({
                          "resultId": "",
                          "provider": "global_menu",
                          "type": "action",
                          "isAction": true,
                          "resultKind": "action",
                          "sectionTitle": "Actions",
                          "name": itemLabel,
                          "path": menuLabel,
                          "intent": menuLabel + " > " + itemLabel,
                          "actionId": "globalmenu:" + menuId + ":" + itemId,
                          "menuId": menuId,
                          "menuItemId": itemId,
                          "iconName": "view-list-symbolic",
                          "score": 170
                      })
        }
    }
    return rows
}

function _setSearchVisible(shell, visible) {
    if (!shell) {
        return
    }
    if (shell.setSearchVisible) {
        shell.setSearchVisible(visible)
        return
    }
    shell.pulseVisible = !!visible
}

function refreshProfileMeta(shell) {
    var svc = _pulseService(shell)
    if (!svc || !svc.activeSearchProfileMeta) {
        shell.pulseProfileMeta = ({})
        return
    }
    var meta = svc.activeSearchProfileMeta()
    shell.pulseProfileMeta = meta ? meta : ({})
}

function refreshTelemetryMeta(shell) {
    var svc = _pulseService(shell)
    if (!svc || !svc.telemetryMeta) {
        shell.pulseTelemetryMeta = ({})
        return
    }
    var meta = svc.telemetryMeta()
    shell.pulseTelemetryMeta = meta ? meta : ({})
}

function refreshTelemetryLast(shell) {
    var svc = _pulseService(shell)
    if (!svc || !svc.activationTelemetry) {
        shell.pulseTelemetryLast = ({})
        return
    }
    var rows = svc.activationTelemetry(1)
    if (rows && rows.length > 0) {
        shell.pulseTelemetryLast = rows[rows.length - 1]
    } else {
        shell.pulseTelemetryLast = ({})
    }
}

function _seedFallbackEmptyQuery(shell, resultsModel, maxSeed) {
    var limit = Number(maxSeed || 24)
    if (limit <= 0) {
        limit = 24
    }
    var fallbackSeed = [
        {
            "name": "Settings",
            "desktopId": "slm-settings.desktop",
            "executable": "slm-settings",
            "iconName": "preferences-system-symbolic"
        },
        {
            "name": "Files",
            "desktopId": "org.gnome.Nautilus.desktop",
            "executable": "nautilus",
            "iconName": "system-file-manager-symbolic"
        },
        {
            "name": "Terminal",
            "desktopId": "org.kde.konsole.desktop",
            "executable": "konsole",
            "iconName": "utilities-terminal-symbolic"
        }
    ]
    for (var fs = 0; fs < fallbackSeed.length && resultsModel.count < limit; ++fs) {
        var f = fallbackSeed[fs]
        resultsModel.append({
                                "resultId": "",
                                "provider": "apps",
                                "type": "app",
                                "isApp": true,
                                "isAction": false,
                                "resultKind": "app",
                                "sectionTitle": "Top Apps",
                                "name": String(f.name || "Application"),
                                "path": String(f.desktopId || f.executable || ""),
                                "absolutePath": "",
                                "iconName": String(f.iconName || "application-x-executable-symbolic"),
                                "isDir": false,
                                "desktopId": String(f.desktopId || ""),
                                "desktopFile": "",
                                "executable": String(f.executable || ""),
                                "iconSource": ""
                            })
    }
    if (resultsModel.count > 0) {
        shell.pulseSelectedIndex = 0
        shell.pulseProviderStats = ({
                                            "apps": Number(resultsModel.count || 0),
                                            "recent": 0,
                                            "seedSource": "fallback-static"
                                        })
        refreshPreview(shell, resultsModel)
    }
}

function refreshResults(shell, resultsModel, forceReload) {
    if (!resultsModel || !resultsModel.clear || !resultsModel.append) {
        if (shell) {
            shell.pulseSelectedIndex = -1
            shell.pulseProviderStats = ({})
            shell.pulsePreviewData = ({})
        }
        return
    }
    var force = !!forceReload
    var appModel = _appModel(shell)
    var appDeckModel = _appDeckModel(shell)
    var fileApi = _fileApi(shell)
    var svc = _pulseService(shell)
    var generation = Number(shell.pulseQueryGeneration || 0)
    var q = String(shell.pulseQuery || "").trim()
    var resultLimit = _pulseResultLimit()
    function isExplicitIconSource(value) {
        var v = String(value || "").trim().toLowerCase()
        return v.indexOf("qrc:/") === 0
               || v.indexOf("file:/") === 0
               || v.indexOf("http://") === 0
               || v.indexOf("https://") === 0
               || v.indexOf("image://") === 0
               || v.indexOf("/") === 0
    }
    function sanitizeIconName(raw) {
        var v = String(raw || "").trim()
        if (v.length <= 0) {
            return ""
        }
        // Handle serialized GIcon strings and multi-name payloads.
        var comma = v.indexOf(",")
        if (comma > 0) {
            v = v.slice(0, comma).trim()
        }
        var parts = v.split(/\s+/)
        for (var i = 0; i < parts.length; ++i) {
            var p = String(parts[i] || "").trim()
            if (p.length <= 0) continue
            if (p === "GThemedIcon" || p === "ThemedIcon" || p === ".") continue
            return p
        }
        return v
    }
    function desktopBase(value) {
        var v = String(value || "").trim().toLowerCase()
        if (v.length <= 0) return ""
        if (v.indexOf("/") >= 0) {
            var parts = v.split("/")
            v = String(parts[parts.length - 1] || "")
        }
        if (v.length > 8 && v.slice(v.length - 8) === ".desktop") {
            v = v.slice(0, v.length - 8)
        }
        return v
    }
    function buildAppIconCatalog(modelA, modelB) {
        var catalog = ({})
        function put(key, iconNameValue, iconSourceValue) {
            var k = String(key || "").trim().toLowerCase()
            if (k.length <= 0 || catalog[k]) return
            catalog[k] = {
                "iconName": String(iconNameValue || ""),
                "iconSource": String(iconSourceValue || "")
            }
        }
        function scan(modelObj, maxScan) {
            if (!modelObj) return
            var rows = []
            var limit = Math.max(64, Number(maxScan || 320))
            if (modelObj.page) {
                rows = modelObj.page(0, limit, "") || []
            } else if (modelObj.get && typeof modelObj.count !== "undefined") {
                var c = Math.min(limit, Number(modelObj.count || 0))
                for (var i = 0; i < c; ++i) {
                    var r = modelObj.get(i)
                    if (r) rows.push(r)
                }
            }
            for (var j = 0; j < rows.length; ++j) {
                var row = rows[j]
                if (!row) continue
                var did = String(row.desktopId || "")
                var dfile = String(row.desktopFile || "")
                var exe = String(row.executable || "")
                var nm = String(row.name || row.display || "")
                var iconN = String(row.iconName || row.icon || "")
                var iconS = String(row.iconSource || "")
                put("did:" + did, iconN, iconS)
                put("dfile:" + dfile, iconN, iconS)
                put("exe:" + exe, iconN, iconS)
                put("name:" + nm, iconN, iconS)
                put("base:" + desktopBase(did), iconN, iconS)
                put("base:" + desktopBase(dfile), iconN, iconS)
            }
        }
        scan(modelA, 340)
        scan(modelB, 340)
        return catalog
    }
    function appIconFromCatalog(row, catalog) {
        if (!row || !catalog) return ({})
        var did = String(row.desktopId || "")
        var dfile = String(row.desktopFile || "")
        var exe = String(row.executable || "")
        var nm = String(row.name || "")
        var path = String(row.path || "")
        var hit = catalog["did:" + did.toLowerCase()]
                  || catalog["dfile:" + dfile.toLowerCase()]
                  || catalog["exe:" + exe.toLowerCase()]
                  || catalog["base:" + desktopBase(path)]
                  || catalog["base:" + desktopBase(did)]
                  || catalog["base:" + desktopBase(dfile)]
                  || catalog["name:" + nm.toLowerCase()]
        return hit ? hit : ({})
    }
    var appIconCatalog = buildAppIconCatalog(appModel, appDeckModel)
    var statCache = ({})
    function cachedStat(pathValue) {
        var p = String(pathValue || "")
        if (p.length === 0) {
            return ({})
        }
        if (statCache[p] !== undefined) {
            return statCache[p]
        }
        var st = (fileApi && fileApi.statPath) ? fileApi.statPath(p) : ({})
        statCache[p] = st
        return st
    }
    function appEntryKey(desktopIdValue, desktopFileValue, executableValue, nameValue) {
        var did = String(desktopIdValue || "").toLowerCase()
        if (did.length > 0) {
            return "id:" + did
        }
        var dfile = String(desktopFileValue || "").toLowerCase()
        if (dfile.length > 0) {
            return "desktop:" + dfile
        }
        var exe = String(executableValue || "").toLowerCase()
        if (exe.length > 0) {
            return "exe:" + exe
        }
        return "name:" + String(nameValue || "").toLowerCase()
    }
    function parseIsoMs(isoValue) {
        var iso = String(isoValue || "")
        if (iso.length <= 0) {
            return 0
        }
        var ms = Date.parse(iso)
        return isNaN(ms) ? 0 : Number(ms)
    }
    function localQueryFallbackRows(queryText, limitValue) {
        var needle = String(queryText || "").trim().toLowerCase()
        var maxRows = Math.max(1, Number(limitValue || 24))
        var out = []
        var seen = ({})
        var sourceOpts = _pulseSearchSourceOptions()
        var sourceBoosts = _pulseRankBoosts()
        var includeApps = !!sourceOpts.includeApps
        var includeRecent = !!sourceOpts.includeRecent
        var appBoost = Number(sourceBoosts.apps || 0)
        var recentBoost = Number(sourceBoosts.recent_files || 0)

        function pushRow(rowKey, rowData) {
            var key = String(rowKey || "")
            if (key.length <= 0 || seen[key]) {
                return
            }
            seen[key] = true
            out.push(rowData)
        }

        function appendAppLikeRowsFromModel(modelObj, sourceTag, maxScan) {
            if (!modelObj) {
                return
            }
            var scanLimit = Math.max(32, Number(maxScan || 96))
            var rows = []
            if (modelObj.page) {
                rows = modelObj.page(0, scanLimit, "") || []
            } else if (modelObj.get && typeof modelObj.count !== "undefined") {
                var c = Math.min(scanLimit, Number(modelObj.count || 0))
                for (var gi = 0; gi < c; ++gi) {
                    var gr = modelObj.get(gi)
                    if (gr) {
                        rows.push(gr)
                    }
                }
            }
            for (var ai = 0; ai < rows.length && out.length < maxRows; ++ai) {
                var app = rows[ai]
                if (!app) continue
                var appName = String(app.name || app.display || "").trim()
                if (appName.length <= 0 || appName.toLowerCase().indexOf(needle) < 0) {
                    continue
                }
                var did = String(app.desktopId || "")
                var dfile = String(app.desktopFile || "")
                var exe = String(app.executable || "")
                var appKey = "app:" + appEntryKey(did, dfile, exe, appName)
                pushRow(appKey, {
                            "resultId": "",
                            "provider": "apps",
                            "type": "app",
                            "isApp": true,
                            "isAction": false,
                            "resultKind": "app",
                            "sectionTitle": "Top Apps",
                            "name": appName,
                            "path": did.length > 0 ? did : exe,
                            "absolutePath": "",
                            "iconName": String(app.iconName || "application-x-executable-symbolic"),
                            "isDir": false,
                            "desktopId": did,
                            "desktopFile": dfile,
                            "executable": exe,
                            "iconSource": String(app.iconSource || ""),
                            "score": Math.max(40, 200 - ai) + appBoost,
                            "_localSource": sourceTag
                        })
            }
        }

        try {
            if (includeApps && appModel && appModel.page) {
                var appRows = appModel.page(0, Math.max(48, maxRows * 3), "")
                for (var ai = 0; ai < appRows.length && out.length < maxRows; ++ai) {
                    var app = appRows[ai]
                    if (!app) continue
                    var appName = String(app.name || "").trim()
                    if (appName.length <= 0 || appName.toLowerCase().indexOf(needle) < 0) {
                        continue
                    }
                    var did = String(app.desktopId || "")
                    var dfile = String(app.desktopFile || "")
                    var exe = String(app.executable || "")
                    var appKey = "app:" + appEntryKey(did, dfile, exe, appName)
                    pushRow(appKey, {
                                "resultId": "",
                                "provider": "apps",
                                "type": "app",
                                "isApp": true,
                                "isAction": false,
                                "resultKind": "app",
                                "sectionTitle": "Top Apps",
                                "name": appName,
                                "path": did.length > 0 ? did : exe,
                                "absolutePath": "",
                                "iconName": String(app.iconName || "application-x-executable-symbolic"),
                                "isDir": false,
                                "desktopId": did,
                                "desktopFile": dfile,
                                "executable": exe,
                                "iconSource": String(app.iconSource || ""),
                                "score": Math.max(40, 180 - ai) + appBoost
                            })
                }
            }
            if (includeApps && out.length < maxRows) {
                appendAppLikeRowsFromModel(appModel, "app-model", Math.max(48, maxRows * 3))
            }
            if (includeApps && out.length < maxRows) {
                appendAppLikeRowsFromModel(appDeckModel, "appdeck-model", Math.max(48, maxRows * 3))
            }
        } catch (eAppsFallback) {
            console.log("[slm-pulse] local app fallback failed:", String(eAppsFallback))
        }

        try {
            if (includeRecent && fileApi && fileApi.recentFiles) {
                var recentRows = fileApi.recentFiles(Math.max(48, maxRows * 3))
                for (var ri = 0; ri < recentRows.length && out.length < maxRows; ++ri) {
                    var rec = recentRows[ri]
                    if (!rec) continue
                    var rp = String(rec.path || "")
                    if (rp.length <= 0) continue
                    var fileName = rp
                    var slashPos = Math.max(fileName.lastIndexOf("/"), fileName.lastIndexOf("\\"))
                    if (slashPos >= 0 && slashPos + 1 < fileName.length) {
                        fileName = fileName.substring(slashPos + 1)
                    }
                    if (fileName.toLowerCase().indexOf(needle) < 0) {
                        continue
                    }
                    var stat = cachedStat(rp)
                    var isDir = !!(stat && stat.ok && stat.isDir)
                    var mime = (stat && stat.ok) ? String(stat.mimeType || "") : ""
                    var fallbackIcon = (stat && stat.ok) ? String(stat.iconName || "") : ""
                    pushRow("path:" + rp.toLowerCase(), {
                                "resultId": "",
                                "provider": "recent",
                                "type": "path",
                                "isApp": false,
                                "isAction": false,
                                "resultKind": isDir ? "folder" : "file",
                                "sectionTitle": "Recent Files",
                                "name": fileName,
                                "path": rp,
                                "absolutePath": rp,
                                "iconName": ShellUtils.pulseIconForMime(mime, isDir, fallbackIcon),
                                "isDir": isDir,
                                "desktopId": "",
                                "desktopFile": "",
                                "executable": "",
                                "iconSource": "",
                                "score": Math.max(20, 110 - ri) + recentBoost
                            })
                }
            }
        } catch (eRecentFallback) {
            console.log("[slm-pulse] local recent fallback failed:", String(eRecentFallback))
        }

        return out
    }
    var queryOptions = _pulseSearchSourceOptions()
    var includeApps = queryOptions.includeApps
    var includeRecent = queryOptions.includeRecent
    if (q.length === 0) {
        var maxSeed = resultLimit
        var seen = ({})
        var appSeedCount = 0
        var recentSeedCount = 0

        try {
            if (includeApps && appModel && appModel.frequentApps) {
                var top = appModel.frequentApps(8)
                for (var ai = 0; ai < top.length; ++ai) {
                    var app = top[ai]
                    if (!app) {
                        continue
                    }
                    var did = String(app.desktopId || "")
                    var dfile = String(app.desktopFile || "")
                    var exe = String(app.executable || "")
                    var appKey = "app:" + appEntryKey(did, dfile, exe, String(app.name || ""))
                    if (appKey === "app:" || seen[appKey]) {
                        continue
                    }
                    seen[appKey] = true
                    resultsModel.append({
                                            "resultId": "",
                                            "provider": "apps",
                                            "type": "app",
                                            "isApp": true,
                                            "isAction": false,
                                            "resultKind": "app",
                                            "sectionTitle": "Top Apps",
                                            "name": String(app.name || "Application"),
                                            "path": did.length > 0 ? did : exe,
                                            "absolutePath": "",
                                            "iconName": String(app.iconName || "application-x-executable-symbolic"),
                                            "isDir": false,
                                            "desktopId": did,
                                            "desktopFile": dfile,
                                            "executable": exe,
                                            "iconSource": String(app.iconSource || "")
                                        })
                    appSeedCount++
                    if (resultsModel.count >= maxSeed) {
                        break
                    }
                }
            }
        } catch (eTopApps) {
            console.log("[slm-pulse] empty-query top-apps seed failed:", String(eTopApps))
        }

        // Fallback when frequent app telemetry is still empty: seed from normal app page.
        try {
            if (includeApps
                    && appSeedCount <= 0
                    && resultsModel.count < maxSeed
                    && appModel
                    && appModel.page) {
                var pageRows = appModel.page(0, Math.max(8, maxSeed), "")
                for (var pi = 0; pi < pageRows.length; ++pi) {
                    var prow = pageRows[pi]
                    if (!prow) {
                        continue
                    }
                    var pdid = String(prow.desktopId || "")
                    var pdfile = String(prow.desktopFile || "")
                    var pexe = String(prow.executable || "")
                    var pkey = "app:" + appEntryKey(pdid, pdfile, pexe, String(prow.name || ""))
                    if (pkey === "app:" || seen[pkey]) {
                        continue
                    }
                    seen[pkey] = true
                    resultsModel.append({
                                            "resultId": "",
                                            "provider": "apps",
                                            "type": "app",
                                            "isApp": true,
                                            "isAction": false,
                                            "resultKind": "app",
                                            "sectionTitle": "Top Apps",
                                            "name": String(prow.name || "Application"),
                                            "path": pdid.length > 0 ? pdid : pexe,
                                            "absolutePath": "",
                                            "iconName": String(prow.iconName || "application-x-executable-symbolic"),
                                            "isDir": false,
                                            "desktopId": pdid,
                                            "desktopFile": pdfile,
                                            "executable": pexe,
                                            "iconSource": String(prow.iconSource || "")
                                        })
                    appSeedCount++
                    if (resultsModel.count >= maxSeed) {
                        break
                    }
                }
            }
        } catch (eAppPage) {
            console.log("[slm-pulse] empty-query app-page seed failed:", String(eAppPage))
        }

        try {
            if (includeRecent
                    && resultsModel.count < maxSeed
                    && fileApi
                    && fileApi.recentFiles) {
                var left = Math.max(0, maxSeed - resultsModel.count)
                var recents = fileApi.recentFiles(Math.max(10, left))
                for (var ri = 0; ri < recents.length; ++ri) {
                    var rec = recents[ri]
                    if (!rec) {
                        continue
                    }
                    var rp = String(rec.path || "")
                    if (rp.length === 0) {
                        continue
                    }
                    var fkey = "file:" + rp.toLowerCase()
                    if (seen[fkey]) {
                        continue
                    }
                    seen[fkey] = true
                    var fileName = rp
                    var slashPos = Math.max(fileName.lastIndexOf("/"), fileName.lastIndexOf("\\"))
                    if (slashPos >= 0 && slashPos + 1 < fileName.length) {
                        fileName = fileName.substring(slashPos + 1)
                    }
                    var stat = cachedStat(rp)
                    var rowIsDir = !!(stat && stat.ok && stat.isDir)
                    var rowMime = (stat && stat.ok) ? String(stat.mimeType || "") : ""
                    var rowFallbackIcon = (stat && stat.ok) ? String(stat.iconName || "") : ""
                    resultsModel.append({
                                            "resultId": "",
                                            "provider": "recent",
                                            "type": "path",
                                            "isApp": false,
                                            "isAction": false,
                                            "resultKind": rowIsDir ? "folder" : "file",
                                            "sectionTitle": "Recent Files",
                                            "name": fileName,
                                            "path": rp,
                                            "absolutePath": rp,
                                            "iconName": ShellUtils.pulseIconForMime(rowMime, rowIsDir, rowFallbackIcon),
                                            "isDir": rowIsDir,
                                            "desktopId": "",
                                            "desktopFile": "",
                                            "executable": "",
                                            "iconSource": ""
                                        })
                    recentSeedCount++
                    if (resultsModel.count >= maxSeed) {
                        break
                    }
                }
            }
        } catch (eRecent) {
            console.log("[slm-pulse] empty-query recents seed failed:", String(eRecent))
        }

        if (resultsModel.count > 0) {
            console.log("[slm-pulse] seed-empty-query apps=", appSeedCount,
                        "recent=", recentSeedCount, "rows=", resultsModel.count)
            shell.pulseSelectedIndex = 0
            shell.pulseProviderStats = ({
                                                "apps": Number(appSeedCount || 0),
                                                "recent": Number(recentSeedCount || 0),
                                                "seedSource": "local-cache"
                                            })
            refreshPreview(shell, resultsModel)
            return
        }
        // Guaranteed non-empty fallback so empty-query UI never looks broken even
        // before telemetry/recent providers warm up.
        if (includeApps) {
            _seedFallbackEmptyQuery(shell, resultsModel, maxSeed)
        }
        if (resultsModel.count > 0) {
            console.log("[slm-pulse] seed-fallback rows=", resultsModel.count,
                        "appModel=", !!appModel, "fileApi=", !!fileApi, "service=", !!svc)
            return
        }
        console.log("[slm-pulse] seed-empty-query-none appModel=", !!appModel,
                    "fileApi=", !!fileApi, "service=", !!svc)
        // If local seed sources are unavailable/empty, continue and query backend
        // with empty text so frequent apps + recent files can still be shown.
    }
    // Allow single-character queries so Pulse feels responsive from first keypress.
    if (q.length > 0 && q.length < 1) {
        shell.pulseLastLoadedQuery = ""
        shell.pulseAppliedGeneration = generation
        shell.pulsePreviewData = ({})
        shell.pulseProviderStats = ({})
        return
    }
    if (!force
            && q === String(shell.pulseLastLoadedQuery || "")
            && shell.pulseAppliedGeneration === generation) {
        return
    }
    resultsModel.clear()
    shell.pulseSelectedIndex = -1
    shell.pulseProviderStats = ({})
    var localFallbackRows = []
    if (!svc || !svc.query) {
        localFallbackRows = localQueryFallbackRows(q, resultLimit)
        if (!localFallbackRows || localFallbackRows.length <= 0) {
            return
        }
    }
    var topAppKeys = ({})
    if (appModel && appModel.frequentApps) {
        var topForBoost = appModel.frequentApps(Math.max(24, resultLimit))
        for (var ti = 0; ti < topForBoost.length; ++ti) {
            var ta = topForBoost[ti]
            if (!ta) {
                continue
            }
            var tkDid = String(ta.desktopId || "").toLowerCase()
            var tkDfile = String(ta.desktopFile || "").toLowerCase()
            var tkExe = String(ta.executable || "").toLowerCase()
            if (tkDid.length > 0) topAppKeys["id:" + tkDid] = true
            if (tkDfile.length > 0) topAppKeys["desktop:" + tkDfile] = true
            if (tkExe.length > 0) topAppKeys["exe:" + tkExe] = true
        }
    }
    var recentPathInfo = ({})
    if (fileApi && fileApi.recentFiles) {
        var recentForBoost = fileApi.recentFiles(Math.max(120, resultLimit * 5))
        for (var rbi = 0; rbi < recentForBoost.length; ++rbi) {
            var rr = recentForBoost[rbi]
            if (!rr) {
                continue
            }
            var rPath = String(rr.path || "").toLowerCase()
            if (rPath.length > 0) {
                recentPathInfo[rPath] = {
                    "recent": true,
                    "lastOpenedMs": parseIsoMs(rr.lastOpened)
                }
            }
        }
    }
    var rows = []
    queryOptions.sourceBoost = _pulseRankBoosts()
    try {
        rows = (svc && svc.query) ? svc.query(q, queryOptions, resultLimit) : localFallbackRows
    } catch (eQueryCall) {
        console.log("[slm-pulse] svc.query failed, fallback local:", String(eQueryCall))
        rows = []
    }
    if (!rows || rows.length === undefined) {
        rows = []
    }
    if (rows.length <= 0 && q.length > 0) {
        rows = localQueryFallbackRows(q, resultLimit)
    }
    if (q.length > 0) {
        if (queryOptions.includeApps) {
            // Always blend a few local app hits so app discovery does not disappear when
            // provider rows are dominated by command/action entries.
            var localBlendRows = localQueryFallbackRows(q, resultLimit)
            var existingAppKeys = ({})
            for (var bi = 0; bi < rows.length; ++bi) {
                var brow = rows[bi]
                if (!brow) continue
                var bType = String(brow.type || "").toLowerCase()
                var bProvider = String(brow.provider || "")
                var bIsApp = bType === "app"
                             || bType === "desktop"
                             || bType === "application"
                             || bProvider === "apps"
                             || String(brow.resultKind || "").toLowerCase() === "app"
                if (!bIsApp) continue
                var bKey = appEntryKey(brow.desktopId, brow.desktopFile, brow.executable, brow.name)
                existingAppKeys[bKey] = true
            }
            var blendedApps = 0
            for (var lb = 0; lb < localBlendRows.length && blendedApps < 8; ++lb) {
                var lrow = localBlendRows[lb]
                if (!lrow) continue
                var lType = String(lrow.type || "").toLowerCase()
                var lProvider = String(lrow.provider || "")
                var lIsApp = lType === "app"
                             || lType === "desktop"
                             || lType === "application"
                             || lProvider === "apps"
                             || String(lrow.resultKind || "").toLowerCase() === "app"
                if (!lIsApp) continue
                var lKey = appEntryKey(lrow.desktopId, lrow.desktopFile, lrow.executable, lrow.name)
                if (existingAppKeys[lKey]) continue
                existingAppKeys[lKey] = true
                rows.push(lrow)
                blendedApps += 1
            }
        }
    }
    var calculatorResult = _pulseCalculatorEvaluate(q)
    if (calculatorResult && calculatorResult.ok) {
        var calculatorResultId = String(calculatorResult.expression || "").toLowerCase().replace(/\s+/g, "")
        var calculatorLabel = String(calculatorResult.formatted || "")
        var calculatorSubtitle = String(calculatorResult.expression || "")
        if (String(calculatorResult.kind || "") === "conversion") {
            calculatorSubtitle = calculatorSubtitle + " -> " + calculatorLabel
        }
        rows.unshift({
                         "resultId": "calc:" + calculatorResultId,
                         "provider": "calculator",
                         "type": "calculator",
                         "isCalculator": true,
                         "isAction": true,
                         "resultKind": "calculator",
                         "sectionTitle": "Calculator",
                         "name": calculatorLabel,
                         "title": calculatorLabel,
                         "path": calculatorResult.expression,
                         "subtitle": calculatorSubtitle,
                         "intent": calculatorResult.expression + " = " + calculatorLabel,
                         "iconName": "accessories-calculator-symbolic",
                         "iconSource": "",
                         "score": 20000,
                         "calculatorKind": String(calculatorResult.kind || "expression"),
                         "expression": calculatorResult.expression,
                         "resultText": calculatorLabel,
                         "copyText": calculatorLabel,
                         "compoundLabel": String(calculatorResult.compoundLabel || ""),
                         "inputValue": Number(calculatorResult.inputValue || 0),
                         "sourceUnit": String(calculatorResult.sourceUnit || ""),
                         "targetUnit": String(calculatorResult.targetUnit || ""),
                         "resultValue": Number(calculatorResult.value || 0)
                     })
        shell.pulseProviderStats = shell.pulseProviderStats || ({})
        shell.pulseProviderStats.calculator = 1
    }
    console.log("[slm-pulse] query=\"" + q + "\" rows=" + Number(rows.length || 0)
                + " svc=" + (!!(svc && svc.query)))
    if (queryOptions.includeActions) {
        var globalMenuRows = _globalMenuSearchRows(q, Math.max(8, Math.min(40, resultLimit)))
        if (globalMenuRows.length > 0) {
            rows = rows.concat(globalMenuRows)
        }
    }
    if (generation !== Number(shell.pulseQueryGeneration || 0)) {
        return
    }
    var groupedApps = []
    var groupedActions = []
    var groupedSettings = []
    var groupedRecent = []
    var groupedFolders = []
    var groupedFiles = []
    var dedupedRows = ({})
    function providerPriority(providerValue) {
        var pv = String(providerValue || "")
        if (pv === "global_menu") return 4
        if (pv === "apps") return 3
        if (pv === "recent") return 2
        return 1
    }
    var nowMs = Date.now()
    for (var i = 0; i < rows.length; ++i) {
        var row = rows[i]
        if (!row) {
            continue
        }
        var rowType = String(row.type || "path").toLowerCase()
        var rowProvider = String(row.provider || "")
        var rowAbsPath = String(row.absolutePath || row.path || "")
        var rowHasAppIdentity = String(row.desktopId || "").trim().length > 0
                                || String(row.desktopFile || "").trim().length > 0
                                || String(row.executable || "").trim().length > 0
        var rowKind = String(row.resultKind || "").toLowerCase()
        var rowIsApp = rowType === "app"
                       || rowType === "desktop"
                       || rowType === "application"
                       || rowProvider === "apps"
                       || rowKind === "app"
                       || rowHasAppIdentity
        var rowIsCalculator = rowType === "calculator"
                              || rowProvider === "calculator"
                              || rowKind === "calculator"
        var rowIsAction = !rowIsApp && (rowType === "action"
                          || rowProvider === "slm_actions"
                          || rowProvider === "global_menu")
        var rowIsSettings = rowType === "settings"
                            || rowType === "module"
                            || rowType === "setting"
                            || rowProvider === "settings"
        var normalizedIsDir = !!row.isDir
        var normalizedIconName = sanitizeIconName(String(row.iconName || "text-x-generic"))
        var normalizedIconSource = String(row.iconSource || "")
        if (!rowIsApp && !rowIsAction && rowAbsPath.length > 0) {
            var st2 = cachedStat(rowAbsPath)
            if (st2 && st2.ok) {
                var stMime = String(st2.mimeType || "")
                var stIsDir = !!st2.isDir
                var stFallbackIcon = String(st2.iconName || normalizedIconName)
                normalizedIsDir = stIsDir
                normalizedIconName = ShellUtils.pulseIconForMime(stMime, stIsDir, stFallbackIcon)
            }
        }
        var baseRank = Number(row.score || 0)
        var boostRank = 0
        if (rowIsApp) {
            var kDid = String(row.desktopId || "").toLowerCase()
            var kDfile = String(row.desktopFile || "").toLowerCase()
            var kExe = String(row.executable || "").toLowerCase()
            if ((kDid.length > 0 && topAppKeys["id:" + kDid])
                    || (kDfile.length > 0 && topAppKeys["desktop:" + kDfile])
                    || (kExe.length > 0 && topAppKeys["exe:" + kExe])) {
                boostRank += 35
            }
        } else {
            var pKey = rowAbsPath.toLowerCase()
            var rMeta = (pKey.length > 0 && recentPathInfo[pKey]) ? recentPathInfo[pKey] : null
            if (rMeta) {
                var openedMs = Number(rMeta.lastOpenedMs || 0)
                if (openedMs > 0 && nowMs > openedMs) {
                    var ageDays = Math.max(0, (nowMs - openedMs) / 86400000.0)
                    var decay = Math.exp((-1.0 * ageDays) / 7.0)
                    boostRank += Math.round(32 * decay)
                } else {
                    boostRank += 10
                }
            }
        }
        var finalRank = baseRank + boostRank
        var sectionTitleRaw = String(row.sectionTitle || "").trim()
        var sectionTitle = sectionTitleRaw.length > 0 ? sectionTitleRaw : "Files"
        if (sectionTitleRaw.length <= 0 && rowIsApp) {
            sectionTitle = "Top Apps"
        } else if (sectionTitleRaw.length <= 0 && rowIsCalculator) {
            sectionTitle = "Calculator"
        } else if (sectionTitleRaw.length <= 0 && rowIsAction) {
            sectionTitle = "Actions"
        } else if (sectionTitleRaw.length <= 0 && rowIsSettings) {
            sectionTitle = "Settings"
        } else if (sectionTitleRaw.length <= 0 && (rowProvider === "recent" || rowProvider === "recent_files")) {
            sectionTitle = "Recent Files"
        } else if (sectionTitleRaw.length <= 0 && normalizedIsDir) {
            sectionTitle = "Folders"
        }
        var sectionId = "suggestions"
        if (sectionTitle === "Top Apps") {
            sectionId = "apps"
        } else if (sectionTitle === "Actions" || sectionTitle === "Settings") {
            sectionId = "actions"
        } else if (sectionTitle === "Recent Files" || sectionTitle === "Folders" || sectionTitle === "Files") {
            sectionId = "files"
        } else if (sectionTitle === "Calculator") {
            sectionId = "best"
        }
        var normalizedType = rowIsApp ? "app"
                                      : (rowIsCalculator ? "calculator"
                                      : (rowIsAction ? "action"
                                                     : (rowIsSettings ? "settings"
                                                                      : (normalizedIsDir ? "folder" : "path"))))
        if (normalizedIconName.length <= 0) {
            normalizedIconName = rowIsApp ? "application-x-executable-symbolic"
                                          : (rowIsCalculator ? "accessories-calculator-symbolic"
                                          : (rowIsAction ? "system-run-symbolic"
                                                         : (normalizedIsDir ? "folder-symbolic"
                                                                            : "text-x-generic-symbolic")))
        }
        if (rowIsApp) {
            var appIconMeta = appIconFromCatalog(row, appIconCatalog)
            var appIconName = sanitizeIconName(String((appIconMeta && appIconMeta.iconName) || ""))
            var appIconSource = String((appIconMeta && appIconMeta.iconSource) || "")
            if (appIconName.length > 0) {
                normalizedIconName = appIconName
            }
            if (appIconSource.length > 0) {
                normalizedIconSource = appIconSource
            }
        }
        // Canonicalize icon source so UI can render deterministically.
        if (!isExplicitIconSource(normalizedIconSource)) {
            normalizedIconSource = ""
        }
        if (normalizedIconSource.length <= 0 && normalizedIconName.length > 0) {
            normalizedIconSource = "image://themeicon/" + normalizedIconName
        }
        var synthesizedDeepLink = String(row.deepLink || "").trim()
        if (synthesizedDeepLink.length <= 0 && rowIsSettings) {
            var moduleId = String(row.moduleId || "").trim()
            var settingId = String(row.settingId || "").trim()
            var resultIdRaw = String(row.resultId || "").trim()
            if (moduleId.length <= 0 || settingId.length <= 0) {
                if (resultIdRaw.indexOf("settings:setting:") === 0) {
                    var payload = resultIdRaw.substring(String("settings:setting:").length)
                    var splitPos = payload.indexOf("/")
                    if (splitPos > 0) {
                        moduleId = payload.substring(0, splitPos)
                        settingId = payload.substring(splitPos + 1)
                    }
                } else if (resultIdRaw.indexOf("settings:module:") === 0) {
                    moduleId = resultIdRaw.substring(String("settings:module:").length)
                }
            }
            if (moduleId.length > 0 && settingId.length > 0) {
                synthesizedDeepLink = "settings://" + moduleId + "/" + settingId
            } else if (moduleId.length > 0) {
                synthesizedDeepLink = "settings://" + moduleId
            }
        }
        var itemRow = {
            "resultId": String(row.resultId || ""),
            "provider": rowProvider,
            "type": normalizedType,
            "isApp": rowIsApp,
            "isCalculator": rowIsCalculator,
            "isAction": rowIsAction,
            "resultKind": rowIsApp ? "app"
                                   : (rowIsCalculator ? "calculator"
                                   : (rowIsAction ? "action"
                                                  : (rowIsSettings ? "settings"
                                                                   : (normalizedIsDir ? "folder" : "file")))),
            "name": String(row.name || ""),
            "path": String(row.path || rowAbsPath),
            "absolutePath": rowAbsPath,
            "deepLink": synthesizedDeepLink,
            "intent": String(row.intent || row.path || ""),
            "actionId": String(row.actionId || ""),
            "menuId": Number(row.menuId || -1),
            "menuItemId": Number(row.menuItemId || -1),
            "iconName": normalizedIconName,
            "isDir": normalizedIsDir,
            "desktopId": String(row.desktopId || ""),
            "desktopFile": String(row.desktopFile || ""),
            "executable": String(row.executable || ""),
            "iconSource": normalizedIconSource,
            "_rank": finalRank,
            "_providerPriority": providerPriority(rowProvider),
            "sectionTitle": sectionTitle,
            "section": sectionId
        }
        if (sectionTitle === "Actions") {
            itemRow.isAction = true
            itemRow.isApp = false
            itemRow.type = "action"
            itemRow.provider = "slm_actions"
            itemRow.resultKind = "action"
        }
        var dedupeKey = ""
        if (rowIsApp) {
            dedupeKey = "app:" + appEntryKey(itemRow.desktopId,
                                             itemRow.desktopFile,
                                             itemRow.executable,
                                             itemRow.name)
        } else if (rowIsAction || itemRow.sectionTitle === "Actions") {
            var actionNameKey = String(itemRow.name || "").trim().toLowerCase()
            var actionIntentKey = String(itemRow.intent || itemRow.path || "").trim().toLowerCase()
            if (actionIntentKey.length <= 0) {
                actionIntentKey = String(itemRow.actionId || "").trim().toLowerCase()
            }
            dedupeKey = "action:" + actionNameKey + "|" + actionIntentKey
        } else {
            dedupeKey = "path:" + String(itemRow.absolutePath || itemRow.path || "").toLowerCase()
        }
        if (dedupeKey.length <= 5) {
            dedupeKey = "raw:" + String(itemRow.name || "") + "|" + String(itemRow.path || "")
        }
        var existing = dedupedRows[dedupeKey]
        if (!existing) {
            dedupedRows[dedupeKey] = itemRow
        } else {
            var existingRank = Number(existing._rank || 0)
            var existingPri = Number(existing._providerPriority || 0)
            if (finalRank > existingRank
                    || (finalRank === existingRank
                        && Number(itemRow._providerPriority || 0) > existingPri)) {
                dedupedRows[dedupeKey] = itemRow
            }
        }
    }
    for (var dedupeKeyIt in dedupedRows) {
        var deduped = dedupedRows[dedupeKeyIt]
        if (!deduped) {
            continue
        }
        if (deduped.sectionTitle === "Top Apps") {
            groupedApps.push(deduped)
        } else if (deduped.sectionTitle === "Actions") {
            groupedActions.push(deduped)
        } else if (deduped.sectionTitle === "Settings") {
            groupedSettings.push(deduped)
        } else if (deduped.sectionTitle === "Recent Files") {
            groupedRecent.push(deduped)
        } else if (deduped.sectionTitle === "Folders") {
            groupedFolders.push(deduped)
        } else {
            groupedFiles.push(deduped)
        }
    }
    groupedApps.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedActions.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedSettings.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedRecent.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedFolders.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedFiles.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    var orderedCandidates = []
    for (var ai2 = 0; ai2 < groupedApps.length; ++ai2) orderedCandidates.push(groupedApps[ai2])
    for (var ac2 = 0; ac2 < groupedActions.length; ++ac2) orderedCandidates.push(groupedActions[ac2])
    for (var st2 = 0; st2 < groupedSettings.length; ++st2) orderedCandidates.push(groupedSettings[st2])
    for (var ri2 = 0; ri2 < groupedRecent.length; ++ri2) orderedCandidates.push(groupedRecent[ri2])
    for (var fi2 = 0; fi2 < groupedFolders.length; ++fi2) orderedCandidates.push(groupedFolders[fi2])
    for (var si2 = 0; si2 < groupedFiles.length; ++si2) orderedCandidates.push(groupedFiles[si2])

    var totalCandidates = orderedCandidates.length
    var providerCap = Math.max(1, Math.floor(totalCandidates * 0.60))
    var providerCounts = ({})
    var skippedByCap = []
    for (var ci = 0; ci < orderedCandidates.length; ++ci) {
        var cand = orderedCandidates[ci]
        if (!cand) continue
        if (String(cand.sectionTitle || "") === "Actions") {
            cand.isAction = true
            cand.isApp = false
            cand.type = "action"
            cand.provider = "slm_actions"
            cand.resultKind = "action"
        }
        var p = String(cand.provider || "unknown")
        var c = Number(providerCounts[p] || 0)
        if (c >= providerCap) {
            skippedByCap.push(cand)
            continue
        }
        providerCounts[p] = c + 1
        resultsModel.append(cand)
    }
    if (resultsModel.count < Math.min(10, totalCandidates)) {
        for (var si3 = 0; si3 < skippedByCap.length; ++si3) {
            if (String(skippedByCap[si3].sectionTitle || "") === "Actions") {
                skippedByCap[si3].isAction = true
                skippedByCap[si3].isApp = false
                skippedByCap[si3].type = "action"
                skippedByCap[si3].provider = "slm_actions"
                skippedByCap[si3].resultKind = "action"
            }
            resultsModel.append(skippedByCap[si3])
        }
    }
    providerCounts["seedSource"] = "provider-query"
    shell.pulseProviderStats = providerCounts
    if (resultsModel.count > 0) {
        shell.pulseSelectedIndex = 0
    }
    console.log("[slm-pulse] committed query=\"" + q + "\" count=" + Number(resultsModel.count || 0))
    shell.pulseLastLoadedQuery = q
    shell.pulseAppliedGeneration = generation
    refreshPreview(shell, resultsModel)
}

function refreshPreview(shell, resultsModel) {
    if (!_pulseSettingBool("pulse.enablePreview", true)) {
        shell.pulsePreviewData = ({})
        return
    }
    if (shell.pulseSelectedIndex < 0 || shell.pulseSelectedIndex >= resultsModel.count) {
        shell.pulsePreviewData = ({})
        return
    }
    var item = resultsModel.get(shell.pulseSelectedIndex)
    if (!item) {
        shell.pulsePreviewData = ({})
        return
    }
    var kind = String(item.resultKind || "")
    var sectionTitle = String(item.sectionTitle || "")
    var isApp = kind === "app"
                || !!item.isApp
                || String(item.type || "") === "app"
                || String(item.provider || "") === "apps"
    var isCalculator = kind === "calculator"
                        || !!item.isCalculator
                        || String(item.type || "") === "calculator"
                        || String(item.provider || "") === "calculator"
    var isAction = sectionTitle === "Actions"
                   || kind === "action"
                   || !!item.isAction
                   || String(item.type || "") === "action"
                   || String(item.provider || "") === "slm_actions"
                   || String(item.provider || "") === "global_menu"
    var isSettings = kind === "settings"
                     || String(item.type || "") === "settings"
                     || String(item.type || "") === "module"
                     || String(item.type || "") === "setting"
                     || String(item.provider || "") === "settings"
    if (isApp) {
        shell.pulsePreviewData = {
            "kind": "app",
            "name": String(item.name || "Application"),
            "iconName": String(item.iconName || "application-x-executable-symbolic"),
            "desktopId": String(item.desktopId || ""),
            "desktopFile": String(item.desktopFile || ""),
            "executable": String(item.executable || "")
        }
        return
    }
    if (isCalculator) {
        shell.pulsePreviewData = {
            "kind": "calculator",
            "name": String(item.name || item.resultText || "Calculator"),
            "expression": String(item.expression || item.subtitle || item.path || ""),
            "result": String(item.resultText || item.name || ""),
            "calculatorKind": String(item.calculatorKind || "expression"),
            "iconName": String(item.iconName || "accessories-calculator-symbolic"),
            "copyText": String(item.copyText || item.resultText || item.name || ""),
            "compoundLabel": String(item.compoundLabel || ""),
            "type": "calculator"
        }
        return
    }
    if (isAction) {
        shell.pulsePreviewData = {
            "kind": "action",
            "name": String(item.name || "Action"),
            "intent": String(item.intent || item.path || ""),
            "iconName": String(item.iconName || "system-run-symbolic"),
            "actionId": String(item.actionId || ""),
            "menuId": Number(item.menuId || -1),
            "menuItemId": Number(item.menuItemId || -1),
            "desktopId": String(item.desktopId || ""),
            "desktopFile": String(item.desktopFile || "")
        }
        return
    }
    if (isSettings) {
        shell.pulsePreviewData = {
            "kind": "settings",
            "name": String(item.name || "Setting"),
            "path": String(item.path || ""),
            "deepLink": String(item.deepLink || ""),
            "iconName": String(item.iconName || "preferences-system-symbolic")
        }
        return
    }
    if (provider === "clipboard" || kind === "clipboard" || String(item.type || "") === "clipboard") {
        var previewMap = item.preview || ({})
        var clipPreview = String(previewMap.preview || item.previewText || item.subtitle || item.name || "")
        shell.pulsePreviewData = {
            "kind": "clipboard",
            "name": String(item.name || "Clipboard item"),
            "title": String(item.name || "Clipboard item"),
            "subtitle": String(item.subtitle || item.sourceApplication || ""),
            "preview": clipPreview,
            "contentType": String(item.clipboardType || previewMap.contentType || ""),
            "isSensitive": !!item.isSensitive,
            "sourceApplication": String(item.sourceApplication || previewMap.sourceApplication || ""),
            "timestamp": Number(item.timestamp || previewMap.timestamp || 0),
            "timestampBucket": Number(item.timestampBucket || previewMap.timestampBucket || 0),
            "iconName": String(item.iconName || "edit-paste-symbolic")
        }
        return
    }
    var p = String(item.absolutePath || item.path || "")
    if (p.length <= 0 || typeof FileManagerApi === "undefined" || !FileManagerApi || !FileManagerApi.statPath) {
        shell.pulsePreviewData = ({})
        return
    }
    var st = FileManagerApi.statPath(p)
    shell.pulsePreviewData = {
        "kind": "path",
        "name": String(item.name || ""),
        "path": p,
        "isDir": !!(st && st.ok && st.isDir),
        "mimeType": (st && st.ok) ? String(st.mimeType || "") : "",
        "size": (st && st.ok) ? Number(st.size || 0) : 0,
        "modified": (st && st.ok) ? String(st.modified || st.lastModified || "") : ""
    }
}

function showActionNotification(summary, body, iconName, timeoutMs) {
    if (typeof NotificationManager === "undefined" || !NotificationManager || !NotificationManager.Notify) {
        return
    }
    NotificationManager.Notify("Slm Desktop",
                               0,
                               String(iconName || "system-search-symbolic"),
                               String(summary || ""),
                               String(body || ""),
                               [],
                               {
                                   "source": "pulse"
                               },
                               Math.max(800, Number(timeoutMs || 1600)))
}

function copyTextToClipboard(text) {
    var value = String(text || "")
    if (value.length <= 0) {
        return false
    }
    if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.copyTextToClipboard) {
        var reply = FileManagerApi.copyTextToClipboard(value)
        return !!(reply && (reply.ok === undefined ? reply : reply.ok))
    }
    return false
}

function focusFromMenu(shell, pulseWindow) {
    if (!shell) {
        return
    }
    _setSearchVisible(shell, true)
    if (_pulseAutoFocusOnOpen()) {
        Qt.callLater(function() {
            if (pulseWindow && pulseWindow.focusSearchField) {
                pulseWindow.focusSearchField()
            }
        })
    }
}

function activateResult(shell, resultsModel, indexValue) {
    var idx = Number(indexValue)
    if (idx < 0 || idx >= resultsModel.count) {
        return
    }
    var item = resultsModel.get(idx)
    if (!item) {
        return
    }
    var rid = String(item.resultId || "")
    var provider = String(item.provider || "")
    if (provider === "global_menu") {
        var menuId = Number(item.menuId || -1)
        var menuItemId = Number(item.menuItemId || -1)
        if (menuId > 0 && menuItemId > 0
                && typeof GlobalMenuManager !== "undefined"
                && GlobalMenuManager
                && GlobalMenuManager.activateMenuItem) {
            _setSearchVisible(shell, false)
            GlobalMenuManager.activateMenuItem(menuId, menuItemId)
            var nowIso = (new Date()).toISOString()
            shell.pulseTelemetryLast = ({
                                                "when": nowIso,
                                                "provider": "global_menu",
                                                "action": "invoke",
                                                "resultType": "action",
                                                "name": String(item.name || ""),
                                                "intent": String(item.intent || ""),
                                                "menuId": menuId,
                                                "menuItemId": menuItemId
                                            })
            var prevPs = shell.pulseProviderStats ? shell.pulseProviderStats : ({})
            var nextPs = ({})
            for (var pKey in prevPs) {
                nextPs[pKey] = prevPs[pKey]
            }
            var ga = Number(nextPs.globalMenuActivations || 0)
            nextPs.globalMenuActivations = ga + 1
            shell.pulseProviderStats = nextPs
            return
        }
    }
    if (provider === "calculator"
            || String(item.type || "") === "calculator"
            || String(item.resultKind || "") === "calculator") {
        var copyValue = String(item.copyText || item.resultText || item.name || "")
        var calcCopyOk = copyTextToClipboard(copyValue)
        if (calcCopyOk) {
            _setSearchVisible(shell, false)
            showActionNotification("Calculator result copied",
                                   copyValue,
                                   "accessories-calculator-symbolic",
                                   1300)
        } else {
            showActionNotification("Calculator copy failed",
                                   "Clipboard is unavailable",
                                   "dialog-error-symbolic",
                                   2200)
        }
        refreshTelemetryLast(shell)
        return
    }
    if (rid.length > 0
            && typeof PulseService !== "undefined"
            && PulseService
            && PulseService.activateResult) {
        if (provider === "clipboard" && PulseService.resolveClipboardResult) {
            _setSearchVisible(shell, false)
            var resolveReply = PulseService.resolveClipboardResult(rid, {
                                                                           "query": String(shell.pulseQuery || ""),
                                                                           "provider": provider,
                                                                           "scope": "pulse",
                                                                           "source_app": "org.slm.pulse",
                                                                           "text": String(shell.pulseQuery || ""),
                                                                           "initiatedByUserGesture": true,
                                                                           "initiatedFromOfficialUI": true,
                                                                           "recopyBeforePaste": true
                                                                       })
            var resolveOk = !!(resolveReply && resolveReply.ok)
            if (!resolveOk) {
                var err = String((resolveReply && resolveReply.error) ? resolveReply.error : "resolve-failed")
                showActionNotification("Clipboard resolve failed",
                                       err,
                                       "dialog-error-symbolic",
                                       2200)
            } else if (shell.pulseNotifyClipboardResolveSuccess) {
                var resolvedTitle = String((resolveReply && resolveReply.title)
                                           ? resolveReply.title : (item.name || "Clipboard item"))
                showActionNotification("Clipboard item ready",
                                       resolvedTitle,
                                       "edit-paste-symbolic",
                                       1400)
            }
            refreshTelemetryLast(shell)
            return
        }
        var resultType = String(item.type || "")
        if (resultType.length <= 0) {
            resultType = !!item.isDir ? "folder" : "file"
        } else if (resultType === "path") {
            resultType = !!item.isDir ? "folder" : "file"
        }
        var action = "open"
        if (resultType === "app" || provider === "apps") {
            action = "launch"
        } else if (resultType === "action" || provider === "slm_actions") {
            action = "invoke"
        }
        var resolvedPath = String(item.absolutePath || item.path || "")
        var contextUris = []
        if (resolvedPath.length > 0) {
            contextUris = [resolvedPath]
        }
        if (_pulseAutoCloseAfterLaunch()) {
            _setSearchVisible(shell, false)
        }
        PulseService.activateResult(rid, {
                                            "query": String(shell.pulseQuery || ""),
                                            "provider": provider,
                                            "result_type": resultType,
                                            "action": action,
                                            "scope": "pulse",
                                            "source_app": "org.slm.pulse",
                                            "text": String(shell.pulseQuery || ""),
                                            "initiatedByUserGesture": true,
                                            "initiatedFromOfficialUI": true,
                                            "selection_count": contextUris.length,
                                            "uris": contextUris
                                        })
        refreshTelemetryLast(shell)
        return
    }
    if (String(item.type || "") === "app") {
        if (_pulseAutoCloseAfterLaunch()) {
            _setSearchVisible(shell, false)
        }
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("app.entry", {
                                       "type": "desktop",
                                       "name": String(item.name || ""),
                                       "desktopId": String(item.desktopId || ""),
                                       "desktopFile": String(item.desktopFile || ""),
                                       "executable": String(item.executable || ""),
                                       "iconName": String(item.iconName || ""),
                                       "iconSource": String(item.iconSource || "")
                                   }, "pulse")
        }
        return
    }
    var p = String(item.absolutePath || item.path || "")
    if (p.length === 0) {
        return
    }
    if (_pulseAutoCloseAfterLaunch()) {
        _setSearchVisible(shell, false)
    }
    if (!!item.isDir) {
        ShellUtils.openDetachedFileManager(shell, p)
        return
    }
    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
        AppCommandRouter.route("filemanager.open", { "target": p }, "pulse")
    }
}

function activateContextAction(shell, resultsModel, indexValue, action) {
    var idx = Number(indexValue)
    var act = String(action || "")
    if (idx < 0 || idx >= resultsModel.count) {
        return
    }
    var itemCurrent = resultsModel.get(idx)
    var isSettings = !!itemCurrent
            && (String(itemCurrent.provider || "") === "settings"
                || String(itemCurrent.type || "") === "settings"
                || String(itemCurrent.type || "") === "module"
                || String(itemCurrent.type || "") === "setting"
                || String(itemCurrent.resultKind || "") === "settings")
    if ((act === "openSetting" || (act === "open" && isSettings))) {
        activateResult(shell, resultsModel, idx)
        return
    }
    if (String(itemCurrent.provider || "") === "clipboard"
            || String(itemCurrent.type || "") === "clipboard"
            || String(itemCurrent.resultKind || "") === "clipboard") {
        if (act === "paste" || act === "open" || act === "resolve") {
            activateResult(shell, resultsModel, idx)
            return
        }
    }
    if (String(itemCurrent.provider || "") === "calculator"
            || String(itemCurrent.type || "") === "calculator"
            || String(itemCurrent.resultKind || "") === "calculator") {
        if (act === "open" || act === "copyResult") {
            activateResult(shell, resultsModel, idx)
            return
        }
        if (act === "copyExpression") {
            var exprValue = String(itemCurrent.expression || itemCurrent.subtitle || itemCurrent.path || "")
            var exprCopied = copyTextToClipboard(exprValue)
            showActionNotification(exprCopied ? "Expression copied" : "Calculator copy failed",
                                   exprCopied ? exprValue : "Clipboard is unavailable",
                                   exprCopied ? "accessories-calculator-symbolic" : "dialog-error-symbolic",
                                   exprCopied ? 1200 : 2200)
            if (exprCopied) {
                _setSearchVisible(shell, false)
            }
            refreshTelemetryLast(shell)
            return
        }
    }
    if (act === "copyDeepLink") {
        if (!itemCurrent) {
            return
        }
        var deepLink = String(itemCurrent.deepLink || "")
        if (deepLink.length <= 0) {
            var rid = String(itemCurrent.resultId || "")
            if (rid.startsWith("settings:setting:")) {
                var payload = rid.substring(String("settings:setting:").length)
                deepLink = "settings://" + payload
            } else if (rid.startsWith("settings:module:")) {
                deepLink = "settings://" + rid.substring(String("settings:module:").length)
            }
        }
        if (deepLink.length <= 0) {
            return
        }
        var copied = false
        if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.copyTextToClipboard) {
            var copiedRes = FileManagerApi.copyTextToClipboard(deepLink)
            copied = !!(copiedRes && copiedRes.ok)
        }
        showActionNotification(copied ? "Deep link copied" : "Failed to copy",
                               copied ? deepLink : "",
                               copied ? "edit-copy-symbolic" : "dialog-error-symbolic",
                               copied ? 1600 : 2200)
        return
    }
    if (act === "launch" || act === "open") {
        activateResult(shell, resultsModel, idx)
        return
    }
    if (act === "pinToAppDeck") {
        var itemPin = resultsModel.get(idx)
        if (!itemPin) {
            return
        }
        var desktopFile = String(itemPin.desktopFile || "")
        if (desktopFile.length <= 0) {
            return
        }
        var pinOk = false
        if (typeof AppDeckModel !== "undefined" && AppDeckModel && AppDeckModel.addDesktopEntry) {
            pinOk = !!AppDeckModel.addDesktopEntry(desktopFile)
        }
        showActionNotification(pinOk ? "Pinned to AppDeck" : "Failed to pin",
                               pinOk ? String(itemPin.name || desktopFile) : "",
                               pinOk ? "list-add-symbolic" : "dialog-error-symbolic",
                               pinOk ? 1500 : 2200)
        return
    }
    if (act === "copyPath" || act === "copyName" || act === "copyUri") {
        var itemCopy = resultsModel.get(idx)
        if (!itemCopy || !!itemCopy.isApp) {
            return
        }
        var sourcePath = String(itemCopy.absolutePath || itemCopy.path || "")
        if (sourcePath.length <= 0) {
            return
        }
        var valueToCopy = sourcePath
        var summaryText = "Path copied"
        if (act === "copyName") {
            var slashPos = Math.max(sourcePath.lastIndexOf("/"), sourcePath.lastIndexOf("\\"))
            valueToCopy = slashPos >= 0 ? sourcePath.substring(slashPos + 1) : sourcePath
            summaryText = "Name copied"
        } else if (act === "copyUri") {
            valueToCopy = "file://" + encodeURIComponent(sourcePath).replace(/%2F/g, "/")
            summaryText = "URI copied"
        }
        var copyOk = false
        if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.copyTextToClipboard) {
            var copyRes = FileManagerApi.copyTextToClipboard(valueToCopy)
            copyOk = !!(copyRes && copyRes.ok)
        }
        showActionNotification(copyOk ? summaryText : "Failed to copy",
                               copyOk ? valueToCopy : "",
                               copyOk ? "edit-copy-symbolic" : "dialog-error-symbolic",
                               copyOk ? 1600 : 2200)
        return
    }
    if (act !== "openContainingFolder") {
        if (act !== "properties") {
            return
        }
        var itemProps = resultsModel.get(idx)
        if (!itemProps) {
            return
        }
        if (isSettings) {
            activateResult(shell, resultsModel, idx)
            return
        }
        var propPath = ""
        if (!!itemProps.isApp) {
            propPath = String(itemProps.desktopFile || itemProps.executable || "")
        } else {
            propPath = String(itemProps.absolutePath || itemProps.path || "")
        }
        if (propPath.length <= 0) {
            return
        }
        var propParent = propPath
        var slashPosProps = Math.max(propPath.lastIndexOf("/"), propPath.lastIndexOf("\\"))
        if (slashPosProps > 0) {
            propParent = propPath.substring(0, slashPosProps)
        }
        _setSearchVisible(shell, false)
        ShellUtils.openDetachedFileManager(shell, propParent)
        ShellUtils.requestDetachedProperties(shell, propPath)
        return
    }

    var item = resultsModel.get(idx)
    if (!item) {
        return
    }
    var p = String(item.absolutePath || item.path || "")
    if (p.length === 0) {
        return
    }
    var lastSlash = Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\"))
    var parentDir = (lastSlash > 0) ? p.substring(0, lastSlash) : "~"
    _setSearchVisible(shell, false)
    ShellUtils.openDetachedFileManager(shell, parentDir)
}
