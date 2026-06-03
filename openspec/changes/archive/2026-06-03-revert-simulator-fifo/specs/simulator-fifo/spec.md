## REMOVED Requirements

### Requirement: 模拟数据源生成温度值

**Reason**: 设计变更，fork+pipe+FIFO 模型废弃，后续改用多线程+共享内存重新实现
**Migration**: 等待新数据源模块实现

### Requirement: 模拟数据源通过模块化生成

**Reason**: 设计变更，模块拆分方式待重设计
**Migration**: 无

### Requirement: 模拟数据源使用匿名管道传递数据

**Reason**: 设计变更，匿名管道传递数据的方式改为共享内存
**Migration**: 无

### Requirement: 模拟数据源写入有名管道 FIFO

**Reason**: 设计变更，FIFO 输出改为多线程模型 + 条件变量
**Migration**: 无

### Requirement: SIGPIPE 信号处理

**Reason**: 设计变更，多线程下不再使用信号处理
**Migration**: 等待新实现
